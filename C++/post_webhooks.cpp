#include <iostream>
#include <string>
using namespace std;
#include <cpp_httplib/httplib.h>
using namespace httplib;
#include <json/json.hpp>
using json = nlohmann::json;

void hook_set(string url);
void hook_del(string url);
string hook_page_get_html();

void post_webhooks_handler(const Request& req, Response& res)
{
   if (req.has_param("del")) {
      auto val = req.get_param_value("del");
      hook_del(val);
   }
   else if (req.has_param("set")) {
      auto val = req.get_param_value("set");
      hook_set(val);
   }

   cout << "Req: " << req.body.c_str() << endl;
   res.set_content(hook_page_get_html(), "text/html; charset=UTF-8");
}