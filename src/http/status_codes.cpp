#include "status_codes.hpp"

using namespace std;

HttpStatusCode::HttpStatusCode() {
	// # 1xx Informational
	(*this)[100] = "Continue";
	(*this)[101] = "Switching Protocols";

	// # 2xx Success
	(*this)[200] = "OK";
	(*this)[201] = "Created";
	(*this)[202] = "Accepted";
	(*this)[203] = "Non - Authoritative Information";
	(*this)[204] = "No Content";
	(*this)[205] = "Reset Content";
	(*this)[206] = "Partial Content";

	// # 3xx Redirection
	(*this)[300] = "Multiple Choices";
	(*this)[301] = "Moved Permanently";
	(*this)[302] = "Found";
	(*this)[303] = "See Other";
	(*this)[304] = "Not Modified";
	(*this)[305] = "Use Proxy";
	(*this)[307] = "Temporary Redirect";

	// # 4xx Client Error
	(*this)[400] = "Bad Request";
	(*this)[401] = "Unauthorized";
	(*this)[402] = "Payment Required";
	(*this)[403] = "Forbidden";
	(*this)[404] = "Not Found";
	(*this)[405] = "Method Not Allowed";
	(*this)[406] = "Not Acceptable";
	(*this)[407] = "Proxy Authentication Required";
	(*this)[408] = "Request Timeout";
	(*this)[409] = "Conflict";
	(*this)[410] = "Gone";
	(*this)[411] = "Length Required";
	(*this)[412] = "Precondition Failed";
	(*this)[413] = "Request Entity Too Large";
	(*this)[414] = "Request - URI Too Long";
	(*this)[415] = "Unsupported Media Type";
	(*this)[416] = "Requested Range Not Satisfiable";
	(*this)[417] = "Expectation Failed";

	// # 5xx Server Error
	(*this)[500] = "Internal Server Error";
	(*this)[501] = "Not Implemented";
	(*this)[502] = "Bad Gateway";
	(*this)[503] = "Service Unavailable";
	(*this)[504] = "Gateway Timeout";
	(*this)[505] = "HTTP Version Not Supported";
};

HttpStatusCode httpStatusCodes;

int httpMethodCount = 8;

string httpMethods[8] = {"GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE"};

iostream *buildResponseBody(const short &statusCode) {
	iostream *stream = new stringstream;
	(*stream) << "<html>" << endl
	          << "<head><title>" << statusCode << " " << httpStatusCodes[statusCode] << "</title></head>" << endl
	          << "<body>" << endl
	          << "<center><h1>" << statusCode << " " << httpStatusCodes[statusCode] << "</h1></center>" << endl
	          << "<hr><center>" NAME "/" VERSION "</center>" << endl
	          << "</body>" << endl
	          << "</html>" << endl;
	return stream;
}

iostream *buildDirectoryListing(const string &path, const string &title) {
	DIR *dir = opendir(path.c_str());
	if (!dir)
		return nullptr;
	iostream *stream = new stringstream;

	(*stream) << "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Index of " << title << "</title>"
	          << "<style> body { font-family: Arial, sans-serif; font-size: 16px; } h1 { text-align: center; } table { width: 100%; border-collapse: collapse; margin-top: 20px; } th, td { padding: 10px; text-align: left; } th { background-color: #eee; } tr:nth-child(even) { background-color: #f2f2f2; } a { color: #000; text-decoration: none; } a:hover { text-decoration: underline; }</style> "
	          << "</head><body><h1> Index of " << title << "</h1><table><thead><tr><th>Name</th><th>Last Modified</th><th>Size(bytes)</th></tr></thead><tbody>"
	          << "<tr><td> <a href=\"../\">../</a></td> <td>-</td> <td>-</td></tr><tr>";
	struct dirent *entry;

	while ((entry = readdir(dir))) {
		if (entry->d_namlen < 1 || entry->d_name[0] == '.')
			continue;

		struct stat info;
		stat((joinPath(path, joinPath("/", entry->d_name))).c_str(), &info);

		string d_name(entry->d_name);
		d_name += S_ISDIR(info.st_mode) ? "/" : "";

		char dateBuffer[0xFF] = {0};
		strftime(dateBuffer, 0xFF, "%d-%b-%Y %H:%M", localtime(&info.st_mtimespec.tv_sec));

		(*stream) << "<td><a href=\"" << d_name << "\">" << d_name << "</a></td> <td>" << dateBuffer << "</td> <td>" << (S_ISDIR(info.st_mode) ? "-" : to_string(info.st_size)) << "</td> </tr>";
	}
	(*stream) << "</tbody></table></body></html>";
	closedir(dir);
	return stream;
}
