#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <regex>

using namespace std;
using namespace fmt;
using nlohmann::json;

CURL *curl;
string base = "https://api.telegram.org/bot";

#define tgURL(x) (base+x).c_str()
#define escape(x) curl_easy_escape(curl, x.c_str(), x.size())
typedef unsigned int uint;

void error(const char *msg)
{
	cout << msg << endl;
	exit(0);
}

size_t writeFunc(char *ptr, size_t size, size_t nmemb, string* data)
{
	data->append(ptr, size*nmemb);
	return size * nmemb;
}

string getUpdates(uint offset)
{
	string response;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_URL, tgURL(
		format("getUpdates?offset={}", offset)));
	curl_easy_perform(curl);
	if (response == "")
		return "{}";
	return response;
}

void sendPhoto(string url, uint chatId, string caption="")
{
	string response;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_URL, tgURL(
		format("sendPhoto?chat_id={}&photo={}&caption={}", chatId, escape(url), escape(caption))));
	curl_easy_perform(curl);
}

void sendMessage (string text, uint chatId)
{
	string response;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_URL, tgURL(
		format("sendMessage?chat_id={}&text={}", chatId, escape(text))));
	curl_easy_perform(curl);
}

int main(int argc, char **argv) {
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (!curl) error("Couldn't initilize curl!!!");
	if (argc == 1)
		error("You must specify token!!!");
	else if (argc != 3)
		cout << "You can specify the proxy: " << argv[0] << " " << argv[1] << " [proxy URI]" << endl;
	else
		curl_easy_setopt(curl, CURLOPT_PROXY, argv[2]);
	base += string(argv[1]) + "/";
	
	unsigned int offset = 0;
	while (1) {
		Sleep(1500);
		auto data = json::parse(getUpdates(offset));
		if (data["ok"] != true) error("getUpdates returned error!!!");
		for (auto i : data["result"])
		{
			smatch m;
			uint chatId;
			string response;
			auto msg = i["message"];
			cout << "Got msg: " << msg["text"] << " from " << msg["chat"]["username" ]<<  endl;
			if (msg["text"] == "/start")
			{
				sendMessage(
					"Hello i am bot. Send me a series name and i will send you it's poster and link on it."
					"Results are taken from hdseria.tv."
					, msg["chat"]["id"]);
			}
			else
			{
				curl_easy_setopt(curl, CURLOPT_URL, "http://hdseria.tv/index.php?do=search");
				string params = format("subaction=search&story={}", msg["text"].get<string>());
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
				curl_easy_perform(curl);
				curl_easy_setopt(curl, CURLOPT_POST, 0);
				regex r("<div class=\"short-img img-box\">\\s+<img src=\"(.*?)\"[\\s\\S]+?href=\"(.*?)\"");
				regex r1(":<\\/div[\\s\\S]+?<br><br>\\s+(.*?)\\s+<\\/div>");
				regex r2("<(.*?)>");
				while (regex_search(response, m, r))
				{
					sendPhoto("http://hdseria.tv" + m[1].str(), msg["chat"]["id"], m[2]);
					smatch m1;
					string response1;
					curl_easy_setopt(curl, CURLOPT_URL, m[2].str().c_str());
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response1);
					curl_easy_perform(curl);
					if (regex_search(response1, m1, r1))
					{
						string desc = m1[1].str();
						desc = regex_replace(desc, r2, "");
						sendMessage(desc, msg["chat"]["id"]);
					}
					else
						cout << "Description not found." << endl;
					response = m.suffix();
				}
				sendMessage("Done.", msg["chat"]["id"]);
			}
			offset = i["update_id"].get<unsigned int>()+1;
		}
	}
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return 0;
}