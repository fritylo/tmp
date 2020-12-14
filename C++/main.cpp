// ИКЛЮДЫ

#include <iostream> // стандартная библиотека ввода вывода
#include <string> // библиотека строк
#include <vector> // динамические массивы
#include <fstream> // файловый поток
#include <sstream> // строковый поток (для чтения из файла)
using namespace std; // используем std

#include <cpp_httplib/httplib.h> // подключаем хттплиб из папки инкслуд
using namespace httplib; // юзаем пространство имён
#include <json/json.hpp> // теперь json
using json = nlohmann::json; // испольуем его

// File::pwd Linux
#include <libgen.h>         // dirname
#include <unistd.h>         // readlink
#include <linux/limits.h>   // PATH_MAX


// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ

Server server; // глобальная переменная сервера
const int SERVER_PORT = 1234; // порт сервера - константа

string pwd; // полный путь к исполнимому файлу

string template_webhook_single; // тут будет текст хтмл шаблона
string template_webhook_page; // тут будет текст хтмл шаблона

string config_path; // тут будет путь к файлу конфига для вебхуков
string speech_path; // путь к файлам с джейсон ответами

json config; // тут будет живой джейсон конфига вебхуков


// FILE API
// Далее несколько функций, в которых лежит код 
// для чтения файлов, записи и проверки существования.
// Все функции начинаются с file_. Особая функция
// file_pwd, она совйственна каждой системе и тут 
// линуксовая версия. На винде возможно не надо будет
// pwd вообще, тогда просто заменить ретёрн на return "";

// получение пути к папке исполнимого файла (линукс)
string file_pwd() { 
	char result[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
	const char* path;
	if (count != -1) {
		path = dirname(result);
	}
	return string(path);
}

// чтение из файла по заданному пути
string file_read(string abspath) {
	string res;
	ifstream file(abspath);
	if (file.good()) {
		res = string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
		file.close();
		return res;
	}
	else {
		file.close();
		cout << "file_read Error: file ifstream is bad. (" << abspath << ")" << endl;
		throw "file_read Error: file ifstream is bad.";
	}
}
// запись в файл по заданному пути
void file_write(string abspath, const string& data) {
	ofstream file;
	file.open(abspath);
	file << data;
	file.close();
}

// чтение в джейсон
json file_read_json(string abspath) {
	return json::parse(file_read(abspath));
}
// запись джейсона в файл
void file_write_json(string abspath, json data) {
	file_write(abspath, data.dump());
}

// проверка файла на существование
bool file_exists(string abspath) {
	ifstream file(abspath);
	bool res = file.good();
	file.close();
	return res;
}


// Replace All
// Эта функция заменяет все вхождения в строке from -> to

string replace_all(string str, const string& from, const string& to)
{
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'fro3m'
	}
	return str;
}


// WEBHOOKS
// Это набор функций для страницы вебхуков.
// Тут есть функции для работы с файлом
// конфига и получения готового HTML по шаблонам.

// обновляет config.json согласно переменной config
void config_write() {
	file_write_json(config_path, config);
}
// обновляет переменную config согласно config.json
void config_read() {
	config = file_read_json(config_path);
}

// определяет индекс вебхука в массиве (для удаления или добавления)
int hook_get_index(string url) {
	int i = 0;
	for (auto& hook : config["webhooks"]) {
		if (url == hook.get<string>()) return i;
		i++;
	}
	return -1; // если ничего не найдено вернёт -1
}
// добавляет новый хук в массив хуков
void hook_set(string url) {
	if (hook_get_index(url) == -1) {
		config["webhooks"].push_back(url);
		config_write();
	}
}
// удаляет хук из массива хуков
void hook_del(string url) {
	int index = hook_get_index(url);
	if (index != -1) {
		config["webhooks"].erase(index);
		config_write();
	}
}

// получение готового HTML страницы хуков
string hook_page_get_html() {
	string webhooks_html = "";
	for (auto& hook : config["webhooks"]) {
		webhooks_html += replace_all(template_webhook_single, "{Webhook URL}", hook) + "\n";
	}
	string res_html = replace_all(template_webhook_page, "{webhooks_list}", webhooks_html);
	return res_html;
}


// ПОДКЛЮЧЕНИЕ РОУТОВ

#include "post_root.cpp"
#include "post_webhooks.cpp"

// MAIN

int main(int argc, char** argv)
{
	try {
		pwd = file_pwd();

		// получаем содержимое шаблонных хтмл файлов и кладем в переменные
		template_webhook_page = file_read(pwd + "/../html/webhook-page.html");
		template_webhook_single = file_read(pwd + "/../html/webhook-single.html");

		// подключение файла конфига
		config_path = pwd + "/../json/config.json"; // определяем путь
		if (!file_exists(config_path)) // если файл не существует
			file_write(config_path, R"({ "webhooks": [] })"); // создаем шаблонный
		config_read(); // теперь читаем его, чтобы обновить json переменную config

		// подключаем роуты
		server.Post("/", post_root_handler); // из файла post_root.cpp
		server.Post("/webhooks", post_webhooks_handler); // из файла post_webhook.cpp

		// запускаем сервер
		cout << "Server served on localhost:" << SERVER_PORT << "\n";
		server.listen("localhost", SERVER_PORT);

		return 0;
	} catch (const char* e) {
		cout << e;
	}
}