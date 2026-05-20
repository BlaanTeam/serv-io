#include "utils.hpp"

using namespace std;

static void label(MainContext<> *main, int &id, ostream &stream) {
	if (!main) return;

	stream << "\tNode" << id << "[label=<<table border=\"0\" cellborder=\"1\">";
	stream << "<tr><td colspan=\"2\"><b>";
	switch (main->type()) {
	case httpCtx:
		stream << "HTTP";
		break;
	case serverCtx:
		stream << "SERVER";
		break;
	case locationCtx:
		stream << "LOCATION";
		break;
	}
	stream << "</b></td> </tr>";

	map<string, vector<string>, StringICaseCompare>::iterator it = main->directives().begin();

	while (it != main->directives().end()) {
		stream << "<tr><td align=\"left\">" << it->first << "</td>";

		stream << "<td>";
		for (size_t idx = 0; idx < it->second.size(); idx++) {
			stream << it->second[idx];
			if (idx + 1 != it->second.size())
				stream << " ";
		}

		stream << "</td> </tr>";

		it++;
	}

	stream << "</table>>, shape=plain]\n";
}

static void edge(int from, int to, ostream &stream) {
	stream << "\tNode" << from << " -> "
	       << "Node" << to << "[wieght=9]\n";
}

static int dotDfs(MainContext<> *main, int &id, ostream &stream) {
	int my_id = 0, i;

	my_id = id++;

	if (main) {
		label(main, my_id, stream);
		for (size_t idx = 0; idx < main->contexts().size(); idx++) {
			i = dotDfs(main->contexts()[idx], id, stream);
			edge(my_id, i, stream);
		}
	}
	return my_id;
}

void dumpConfigDot(MainContext<> *main, ostream &stream) {
	if (!main) return;
	int id = 0;
	stream << "digraph {\n";
	dotDfs(main, id, stream);
	stream << "}\n";
}

string getUTCDate(void) {
	time_t clock;
	time(&clock);
	tm *date = gmtime(&clock);

	char dateBuffer[0xFF] = {0};
	strftime(dateBuffer, 0xFF, "%a, %d %b %Y %H:%M:%S GMT", date);
	return string(dateBuffer);
}

long long getmstime(void) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1e3) + (tv.tv_usec / 1e3);
}
