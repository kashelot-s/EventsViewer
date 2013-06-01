#include <stdio.h>
#include "gui.h"

int main(int argc, char *argv[])
{
	struct EventsDb eventsdb;
	enum EventsDb_Error err_eventdb;
	int status;

	err_eventdb = EventsDb_Init(&eventsdb);
	if (EVENTSDB_OK != err_eventdb) {
		printf("Compiling default regexep fails\n");
		return -1;
	}
	status = gui_main(argc, argv, &eventsdb);
	EventsDb_Done(&eventsdb);

	return status;
}

