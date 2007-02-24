#include <pb_view.h>
#include <pb_controller.h>
#include <poddlthread.h>
#include <dllist.h>
#include <download.h>
#include <config.h>
#include <logger.h>

#include <sstream>
#include <iostream>

using namespace podbeuter;
using namespace newsbeuter;

pb_view::pb_view(pb_controller * c) : ctrl(c), dllist_form(dllist_str), keys(0) { 
}

pb_view::~pb_view() { 
	stfl::reset();
}

void pb_view::run() {
	bool quit = false;
	bool auto_download = false;

	do {

		if (ctrl->view_update_necessary()) {

			char buf[1024];
			snprintf(buf, sizeof(buf), _("Queue (%u downloads in progress, %u total)"), ctrl->downloads_in_progress(), ctrl->downloads().size());

			dllist_form.set("head", buf);

			GetLogger().log(LOG_DEBUG, "pb_view::run: updating view... downloads().size() = %u", ctrl->downloads().size());

			if (ctrl->downloads().size() > 0) {

				std::string code = "{list";
				
				unsigned int i = 0;
				for (std::vector<download>::iterator it=ctrl->downloads().begin();it!=ctrl->downloads().end();++it,++i) {
					char buf[1024];
					std::ostringstream os;
					snprintf(buf, sizeof(buf), " %4u [%5.1f %%] %-20s %s -> %s", i+1, it->percents_finished(), it->status_text(), it->url(), it->filename());
					os << "{listitem[" << i << "] text:" << stfl::quote(buf) << "}";
					code.append(os.str());
				}

				code.append("}");

				dllist_form.modify("dls", "replace_inner", code);
			}

			ctrl->set_view_update_necessary(false);
		}

		const char * event = dllist_form.run(500);

		if (auto_download) {
			if (ctrl->get_maxdownloads() > ctrl->downloads_in_progress()) {
				ctrl->start_downloads();
			}
		}

		if (!event || strcmp(event,"TIMEOUT")==0) continue;

		operation op = keys->get_operation(event);

		if (dllist_form.get("msg").length() > 0) {
			dllist_form.set("msg", "");
			ctrl->set_view_update_necessary(true);
		}

		switch (op) {
			case OP_PB_TOGGLE_DLALL:
				auto_download = !auto_download;
				break;
			case OP_QUIT:
				if (ctrl->downloads_in_progress() > 0) {
					dllist_form.set("msg", _("Error: can't quit: download(s) in progress."));
					ctrl->set_view_update_necessary(true);
				} else {
					quit = true;
				}
				break;
			case OP_PB_DOWNLOAD: {
					std::istringstream os(dllist_form.get("dlposname"));
					int idx = -1;
					os >> idx;
					if (idx != -1) {
						if (ctrl->downloads()[idx].status() != DL_DOWNLOADING) {
							poddlthread * thread = new poddlthread(&ctrl->downloads()[idx]);
							thread->start();
						}
					}
				}
				break;
			case OP_PB_CANCEL: {
					std::istringstream os(dllist_form.get("dlposname"));
					int idx = -1;
					os >> idx;
					if (idx != -1) {
						if (ctrl->downloads()[idx].status() == DL_DOWNLOADING) {
							ctrl->downloads()[idx].set_status(DL_CANCELLED);
						}
					}
				}
				break;
			case OP_PB_DELETE: {
					std::istringstream os(dllist_form.get("dlposname"));
					int idx = -1;
					os >> idx;
					if (idx != -1) {
						if (ctrl->downloads()[idx].status() != DL_DOWNLOADING) {
							ctrl->downloads()[idx].set_status(DL_DELETED);
						}
					}
				}
				break;
			case OP_PB_PURGE:
				if (ctrl->downloads_in_progress() > 0) {
					dllist_form.set("msg", _("Error: unable to perform operation: download(s) in progress."));
				} else {
					ctrl->reload_queue();
				}
				ctrl->set_view_update_necessary(true);
				break;
			default:
				break;
		}

	} while (!quit);
}