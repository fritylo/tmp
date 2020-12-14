#include <iostream>
#include <string>
using namespace std;
#include <cpp_httplib/httplib.h>
using namespace httplib;
#include <json/json.hpp>
using json = nlohmann::json;

string get_command(json yandex_json);
string get_product_name(json yandex_json, int product_token_index);
json get_product_set(json yandex_json, int product_token_index);
void remove_product(int product_token_index, json& yandex_json, json& state);


// путь к папке с json файлами ответов
const string SPEECH_JSON_PATH = "/../json/speech";
json state; // хранилище session state, его изменение влияет на конечный state ответа
string post_root_ans; // переменная для ответа Яндексу

// Генерирует ответ Яндексу
// speech_name - это путь в папке речей
// config - настраивает работу функции, например может добавить кнопки
void speech(string speech_name, json config = R"({})"_json) {
   // читаем базовый ответ из папки спич
   string target_json_string = file_read(pwd + SPEECH_JSON_PATH + "/" + speech_name + ".json");
   // парсим в джейсон
   json target_dialog = json::parse(target_json_string);
   // переменная state получает своё значение в post_root_handler
   // если стейт есть, то мы не на начале сессии, а продолжаем
   bool not_startup = state != nullptr;

   cout << "Speech: " << speech_name << endl; // отладочная инфа

   if (not_startup) { // если мы продолжаем сессию
      if (config.contains("buttons")) { // если конфиг влияет на кнопки
         auto buttons = config["buttons"].get<string>(); // берём кнопки

         if (buttons == "speak") { // если пресет кнопок для речи (говорит/молчать)
            if (!state["speak"].get<bool>()) { // в зависимости от режима речи выбираем кнопку
               target_dialog["response"].erase("tts"); // в теории чабанова затирание ттс должно было убирать звук
               // по умолчанию не все кнопки присутствуют, динамические кнопки появляются в этой функции
               target_dialog["response"]["buttons"].push_back(R"({ "title": "Говорить" })"_json); // добавление кнопки
            }
            else {
               target_dialog["response"]["buttons"].push_back(R"({ "title": "Молчать" })"_json);
            }
            target_dialog["response"]["buttons"].push_back(R"({ "title": "Корзина" })"_json);
         }
         else if (buttons == "cart") { // если используются кнопки корзины
            json buttons_cart = R"([
               { "title": "Очистить корзину" },
               { "title": "Добавить в корзину" },
               { "title": "Удалить из корзины" },
               { "title": "Что в корзине" },
               { "title": "Сумма" },
               { "title": "Покупка завершена" }
            ])"_json;

            for (auto& button : buttons_cart) { // подключаем каждую кнопку в цикле
               target_dialog["response"]["buttons"].push_back(button);
            }
         }
      }

      // вся справочная информация находится в файле 
      // help_all.json, из него надо достать один объект и ответом на нужный
      // запрос о помощи, для этого при вызове функции задается параметр help_all_name
      if (config.contains("help_all_name")) {
         string help_text, help_tts;
         auto help_name = config["help_all_name"].get<string>();

         auto help_arr = json::parse(target_dialog["help_arr"].dump());
         target_dialog.erase("help_arr");

         // ищем требуемую справку
         for (auto it = help_arr.begin(); it != help_arr.end(); ++it) {
            json help_curr = *it;
            auto help_name_curr = help_curr["name"].get<string>();
            if (help_name_curr == help_name) {
               help_text = help_curr["text"].get<string>();
               help_tts = help_curr["tts"].get<string>();
               break;
            }
         }

         // когда нашли выводим что нашли
         cout << "help::text: " << help_text << endl;
         cout << "help::tts: " << help_tts << endl;
         // затем затираем стандартный текст
         target_dialog["response"]["text"] = help_text;
         target_dialog["response"]["tts"] = help_tts;
      }

      // если конфиг переопределяет текст
      if (config.contains("text")) {
         auto newText = config["text"].get<string>();
         target_dialog["response"]["text"] = newText;
         target_dialog["response"]["tts"] = newText;
      }

      // применение изменений стейта в ответе яндексу
      target_dialog["session_state"] = state;
   }
   else { // если это начало сессии
      // генерируем стандартный ответ
      state = R"({
            "speak": true,
            "cart": [],
            "mode": "normal"
         })"_json;

      target_dialog["response"]["buttons"].push_back(R"({ "title": "Молчать" })"_json);
      target_dialog["session_state"] = state;
   }

   // все проделанные операии сохраняются в эту глобальную переменную
   post_root_ans = target_dialog.dump(); // она же используется как ответ сервера
}

// HANDLER
// Эта функция управляет поведением при переходе в различные состояния.
// Она использует speech функцию для генерации кода и по суть один большой
// if-else.
void post_root_handler(const Request& req, Response& res)
{
try {
   cout << "////////////////" << endl;

   // джейсон который нам прислал яндекс
   auto yandex_json = json::parse(req.body);

   // если новая сессия
   if (yandex_json["session"]["new"].get<bool>()) {
      speech("startup", nullptr);
   } // если не продолжение сессии
   else {
      state = yandex_json["state"]["session"]; // обновляем стейт из пришедшего

      // команда приходит в разных форматах время от времени
      // поэтому для получения команды используется функция
      string command = get_command(yandex_json);
      cout << "\nЗапрос: " << command << "\n" << endl; // отладочная информация
      // яндекс присылает два варианта команды - в точности и упрощенную, это в точности, выше упрощенная
      string command_orig = yandex_json["request"]["original_utterance"].get<string>();

      // проверка правильности данных сессии в соответствии с ТЗ чабана
      if (!(state.contains("cart") && state.contains("speak") && state.contains("mode"))) {
         speech("error"); // если все плохо говорим об ошибке
      }
      else { // если же данные сессии в порядке
         auto cart = state["cart"];
         auto speak = state["speak"];
         auto mode = state["mode"];

         // параметр wait используется для обработки команд типа "добавить в корзину"
         // она может быть использована прямо "добавить в корзину огурцы 25 рублей"
         // или в два этапа "добавить в корзину" + "огурцы 25 рублей"
         // для второго случая используется параметр ожидания
         bool is_wait = state.contains("wait") && !state["wait"].is_null();
         string wait = is_wait ? state["wait"].get<string>() : "";

         // конфиги для разных типов кнопок
         auto buttons_normal = R"({ 
               "buttons": "speak"
            })"_json;
         auto buttons_cart = R"({ 
               "buttons": "cart"
            })"_json;

         // число слов в запросе юзера
         int tokens_count = yandex_json["request"]["nlu"]["tokens"].size();

         // если мы в нормальном режиме (стартовый)
         if (mode == "normal") {
            if (false) {
            }
            else if (command == "молчать") {
               state["speak"] = false;
               speech("mode/silent", buttons_normal);
            }
            else if (command == "говорить") {
               state["speak"] = true;
               speech("mode/speak", buttons_normal);
            }
            else if (command == "помощь") {
               state["mode"] = "help";
               speech("help/start");
            }
            else if (command == "корзина") {
               state["mode"] = "cart";
               speech("cart/start", buttons_cart);
            }
            else {
               speech("unknown", buttons_normal);
            }
         }

         // если мы в режиме помощи
         else if (mode == "help") {
            if (command == "назад") {
               state["mode"] = "normal";
               speech("help/end", buttons_normal);
            }
            else {
               speech("help/all", json::parse("{ \"help_all_name\": \"" + command_orig + "\" }"));
            }
         }

         // если мы в режиме корзины
         else if (mode == "cart") {
            if (false) {
            }

            else if (command == "очистить корзину") {
               state["cart"] = json::array();
               speech("cart/skill/clean", buttons_cart);
            }

            else if ((is_wait && wait == "добавить в корзину") || command.rfind("добавить в корзину", 0) == 0) {
               if (!is_wait) {
                  if (tokens_count == 3) {
                     state["wait"] = "добавить в корзину";
                     speech("cart/skill/add_no_args");
                  }
                  else {
                     // эта функция получает называние продукта по номеру в словах пользователя
                     json productData = get_product_set(yandex_json, 3);
                     state["cart"].push_back(productData);
                     speech("cart/skill/add", buttons_cart);
                  }
               }
               else {
                  state["wait"] = nullptr;

                  json productData = get_product_set(yandex_json, 0);
                  state["cart"].push_back(productData);
                  speech("cart/skill/add", buttons_cart);
               }
            }

            else if ((is_wait && wait == "удалить из корзины") || command.rfind("удалить из корзины", 0) == 0) {
               if (!is_wait) {
                  if (tokens_count == 3) {
                     state["wait"] = "удалить из корзины";
                     speech("cart/skill/remove_no_args");
                  }
                  else {
                     // эта функция удаляет продукт из корзины в стейте
                     // для получения названия использует индекс в словах юзера
                     // в данном случае 3
                     remove_product(3, yandex_json, state);
                     speech("cart/skill/remove", buttons_cart);
                  }
               }
               else {
                  state["wait"] = nullptr;
                  remove_product(0, yandex_json, state);
                  speech("cart/skill/remove", buttons_cart);
               }
            }

            else if (command == "что в корзине") {
               auto cart = state["cart"];

               if (cart.empty()) speech("cart/skill/list_free", buttons_cart);
               else {
                  // генерация текста корзины
                  string cartText = "Корзина выглядит так: \\n";
                  for (auto& product : cart) {
                     auto name = product["name"].get<string>();
                     int priceVal = product["price"].get<int>();
                     auto price = to_string(priceVal);
                     cartText += name + " : " + price + "\\n";
                  }
                  json configList = {
                     { "text", cartText },
                     { "buttons", "cart" },
                  };
                  speech("cart/skill/list", configList);
               }
            }

            else if (command == "сумма") {
               int cartSum = 0;
               // подсчет суммы
               for (auto& product : state["cart"]) {
                  cartSum += product["price"].get<int>();
               }

               json configSum = {
                     { "text", "Суммарная стоимость: " + to_string(cartSum) },
                     { "buttons", "cart" },
               };
               speech("cart/skill/sum", configSum);
            }

            else if (command == "покупка завершена") {
               // получение id юзера
               string id = yandex_json["session"]["user"].contains("user_id")
                  ? yandex_json["session"]["user"]["user_id"].get<string>()
                  : "anonymous"; // он может быть анонимным
               // это массив всех товаров юзера (как чек в магазине)
               json check = {
                  {"user_id", id},
                  {"check", state["cart"]},
               };

               // преобразуем json в строку
               string checkString = check.dump();

               // отправляем каждому вебхуку этот джейсон
               for (auto& hook : config["webhooks"]) {
                  string hookString = hook.get<string>();
                  Client sender{ hookString.c_str() };
                  // само отправление пост запроса
                  sender.Post("/", checkString, "application/json; charset=utf8");
               }

               state["mode"] = "normal";
               speech("cart/end", buttons_normal);
            }

            else {
               speech("unknown", buttons_cart);
            }
         }
      }
   }

   cout << req.body.c_str() << endl; // вывод джейсона пришедшего от яндекса
   // вывод сгенерированного функцией speech в ответ яндексу
   res.set_content(post_root_ans, "text/json; charset=UTF-8");
} catch (const char* e) {
   cout << e;
}
}

// Ниже представлены вспомогательные функции

string get_command(json yandex_json) {
   string command;
   if (yandex_json["request"].contains("command")) {
      command = yandex_json["request"]["command"].get<string>();
   }
   else {
      command = "";
      for (auto& token : yandex_json["request"]["nlu"]) {
         command += token.get<string>();
      }
   }
   return command;
}

string get_product_name(json yandex_json, int product_token_index) {
   return yandex_json["request"]["nlu"]["tokens"][product_token_index].get<string>();
}

json get_product_set(json yandex_json, int product_token_index) {
   string product_name = get_product_name(yandex_json, product_token_index);

   json entities = yandex_json["request"]["nlu"]["entities"];
   int product_price;
   for (auto it = entities.begin(); it != entities.end(); ++it) {
      if ((*it)["type"] == "YANDEX.NUMBER") {
         product_price = (*it)["value"].get<int>();
         break;
      }
   }

   auto res = json{
      {"name", product_name},
      {"price", product_price},
   };
   return res;
}

void remove_product(int product_token_index, json& yandex_json, json& state) {
   json new_cart = json::array();

   string product_name = get_product_name(yandex_json, product_token_index);
   int i = 0;
   for (auto& product : state["cart"]) {
      string product_nameCurr = product["name"].get<string>();
      if (product_name != product_nameCurr) {
         new_cart.push_back(product);
      }
      i++;
   }

   state["cart"] = new_cart;
}