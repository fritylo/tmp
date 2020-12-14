from flask import Flask, request # flask это библиотека для сервака
import openpyxl # для работы с экселем, качается отдельно: pip install openpyxl
import os.path # для проверки существования файла
import datetime # для получения текущей даты

# создаем объект приложения
app = Flask(__name__)

#STORAGE_LEN = 1000
STORAGE_LEN = 0 # константа для максимального размера хранилища
storage = [] # само хранилище - простй массив

# это "декоратор" - спец. синтаксис позволяющий творить чудеса
# в данном случае он привязывает нашу функцию слушать /
@app.route('/', methods=['POST', 'GET'])
def index():
   global storage # глобал открывает переменные из глобально области для редактирования
   # если поступил пост запрос
   if request.method == 'POST': 
      # добавляем данные в хранилище
      storage += [request.json]
      print('req.body:', storage[-1]) # выводим добавленное
      # если хранилище переполнено
      if (len(storage) > STORAGE_LEN):
         save_excel() # сохраняем в экселевский файл
         storage = [] # и чистим хранилище
      # согласно ТЗ возвращаем ОК
      return 'OK' 
      
   elif request.method == 'GET':
      return 'Это GET запрос'

# функция сохранения содержимого в эксель
def save_excel():
   global storage # открывает хранилище из глобальной области видимости
   # константа пути к экселевскому файлу
   STORAGE_FILE = './Python/data.xlsx'
   # создадим переменную для книги экселя
   book = None
   # если файл экселя не существует
   if not os.path.exists(STORAGE_FILE):
      book = openpyxl.Workbook() # создаем пустую книгу
      # добавляем в неё заголовки
      book.active['A1'] = 'N'
      book.active['B1'] = 'User ID'
      book.active['C1'] = 'Datetime'
      book.active['D1'] = 'Item'
      book.active['E1'] = 'Prise'
      book.save(STORAGE_FILE) # и сохраняем
   else: # если же существовал
      book = openpyxl.open(STORAGE_FILE) # то открываем его
   sheet = book.active # устанавливаем первый лист основным
   
   max_row = len(sheet['A']) # определяем сколько строк уже есть в файле, А это колонка
   nowtime = datetime.datetime.now() # берём текущее время
   
   row = max_row + 1 # перемещаем курсор текущего ряда с последнего ряда
   for dataset in storage: # для каждой записи в хранилище
      id = dataset['user_id'] # берём id
      check = dataset['check'] # берём чек (в нем несколько товаров)
      
      for item in check: # для каждого товара из чека
         # выводим информацию на лист
         sheet[row][0].value = row - 1
         sheet[row][1].value = id
         sheet[row][2].value = nowtime
         sheet[row][3].value = item['name']
         sheet[row][4].value = item['price']
         row += 1
   
   book.save(STORAGE_FILE) # в конце сохраняем
   book.close() # и закрываем файл



if __name__ == '__main__':
   app.run()