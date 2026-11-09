#include <algorithm>
#include <cstdio>
#include <cstring>

#include "engine.hpp"
#include "utils.hpp"

const struct pw_registry_events SysAudioLinux::registry_events = {
	.version = PW_VERSION_REGISTRY_EVENTS,
	.global = &SysAudioLinux::registry_global_cb,
	.global_remove = &SysAudioLinux::registry_global_remove_cb,
};

void SysAudioLinux::registry_global_cb(
	void *app,
	uint32_t id,
	uint32_t /*permissions*/,
	const char *type,
	uint32_t /*version*/,
	const struct spa_dict *props
) {
	static_cast<SysAudioLinux *>(app)->handle_registry_global(id, type, props);
}

void SysAudioLinux::registry_global_remove_cb(void *app, uint32_t id) {
	static_cast<SysAudioLinux *>(app)->handle_registry_global_remove(id);
}

void SysAudioLinux::try_link_ports() {
	if (stream_ports.empty() || capture_stream_node_id == 0) {
		return;
	}

	// iterate through the ports of apps that are waiting to be connected
	for (auto it = pending_ports.begin(); it != pending_ports.end();) {
		// remove pending ports for nodes that are not in target_nodes
		if (!target_nodes.count(it->node_id)) {
			it = pending_ports.erase(it);
			continue;
		}

		// check if our self stream has a port for this channel type (EG: "FL", "FR", etc.)
		auto self_port = stream_ports.find(it->channel);

		if (self_port != stream_ports.end()) {
			char out_node[16], out_port[16], in_node[16], in_port[16];

			snprintf(out_node, sizeof(out_node), "%u", it->node_id);
			snprintf(out_port, sizeof(out_port), "%u", it->port_id);
			snprintf(in_node, sizeof(in_node), "%u", capture_stream_node_id);
			snprintf(in_port, sizeof(in_port), "%u", self_port->second);

			struct spa_dict_item items[] = {
				SPA_DICT_ITEM_INIT(PW_KEY_LINK_OUTPUT_NODE, out_node),
				SPA_DICT_ITEM_INIT(PW_KEY_LINK_OUTPUT_PORT, out_port),
				SPA_DICT_ITEM_INIT(PW_KEY_LINK_INPUT_NODE, in_node),
				SPA_DICT_ITEM_INIT(PW_KEY_LINK_INPUT_PORT, in_port),
				SPA_DICT_ITEM_INIT(PW_KEY_LINK_PASSIVE, "true")
			};
			struct spa_dict props = SPA_DICT_INIT(items, 5);

			pw_proxy *link = static_cast<pw_proxy *>(pw_core_create_object(
				core, "link-factory", PW_TYPE_INTERFACE_Link, PW_VERSION_LINK, &props, 0
			));

			if (link) {
				printf("Linking Node[%u]:%s to Stream[%u]\n", it->node_id, it->channel.c_str(), capture_stream_node_id);
				active_links[it->node_id].push_back({ link, it->node_id, it->port_id, it->channel });
			}

			it = pending_ports.erase(it);
		} else {
			++it;
		}
	}
}

void SysAudioLinux::handle_registry_global(uint32_t id, const char *type, const struct spa_dict *props) {
	if (!props) {
		return;
	}

	if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
		const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
		const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
		const char *pid_str = spa_dict_lookup(props, PW_KEY_APP_PROCESS_ID);

		// identify our own streams by name and never route them back into
		// the capture stream (that would capture the audio we are playing)
		if (name && strcmp(name, node_name.c_str()) == 0) {
			if (capture_stream_node_id != id) {
				stream_ports.clear();
				capture_stream_node_id = id;
			}

			return;
		}

		if (name && strcmp(name, output_node_name.c_str()) == 0) {
			output_stream_node_id = id;
			return;
		}

		if (id == capture_stream_node_id || id == output_stream_node_id) {
			return;
		}

		// if is not an audio output stream, skip it
		if (media_class && strcmp(media_class, "Stream/Output/Audio") == 0) {
			pid_t app_pid = 0;
			if (pid_str) {
				app_pid = (pid_t)atoi(pid_str);

				// skip the addon process and any process owned by the app
				if (app_pid == self_pid ||
					sysaudio::utils::is_pid_descendant_of(app_pid, excluded_pid)) {
					return;
				}
			}

			printf("Found Node[%u]\n", id);
			printf("  name=%s, media_class=%s\n", name ?: "unknown", media_class ?: "unknown");

			// insert the node id into the target set with its pid
			target_nodes[id] = app_pid;
		}

		return;
	}

	if (strcmp(type, PW_TYPE_INTERFACE_Port) == 0) {
		const char *dir = spa_dict_lookup(props, PW_KEY_PORT_DIRECTION);

		if (!dir) {
			return;
		}

		uint32_t node_id = (uint32_t)atoi(spa_dict_lookup(props, PW_KEY_NODE_ID) ?: "0");

		// fetch the channel (or use the name if no channel is available)
		const char *chan = spa_dict_lookup(props, PW_KEY_AUDIO_CHANNEL);

		if (!chan) {
			chan = spa_dict_lookup(props, PW_KEY_PORT_NAME);
		}

		std::string chan_str = chan ? chan : "unknown";

		// update the stream_ports if the node is ours and the direction is input
		if (node_id == capture_stream_node_id && strcmp(dir, "in") == 0) {
			stream_ports[chan_str] = id;
			try_link_ports();
		}

		// if the node is a target node and the direction is output, add it to pending_ports
		else if (target_nodes.count(node_id) && strcmp(dir, "out") == 0) {
			pending_ports.push_back({ node_id, id, chan_str });
			try_link_ports();
		}

		return;
	}
}

void SysAudioLinux::handle_registry_global_remove(uint32_t id) {
	if (id == capture_stream_node_id) {
		printf("Capture stream Node[%u] removed\n", id);

		capture_stream_node_id = 0;
		stream_ports.clear();

		return;
	}

	if (id == output_stream_node_id) {
		printf("Output stream Node[%u] removed\n", id);

		output_stream_node_id = 0;

		return;
	}

	// check if the removed node was a target node
	if (target_nodes.count(id)) {
		printf("Unlinking target Node[%u]\n", id);

		target_nodes.erase(id);

		// destroy any active links for this node
		for (auto &active_link : active_links[id]) {
			pw_proxy_destroy(active_link.link);
		}

		active_links.erase(id);

		// remove any pending ports for this node if is in there
		pending_ports.erase(
			std::remove_if(pending_ports.begin(), pending_ports.end(), [id](const PendingPort &p) { return p.node_id == id; }),
			pending_ports.end()
		);
	}
}
