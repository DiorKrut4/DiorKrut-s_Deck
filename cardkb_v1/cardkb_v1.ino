/*
  ESP32-S3-N16R8 + M5 Unit CardKB v1.1 (I2C) + TFT SPI 240x320 (ILI9341)
  ---------------------------------------------------------------------
  Версия 3: буфер экрана в памяти + мигающий курсор + перемещение
  курсора стрелками + Esc "выключает/включает" экран (содержимое
  сохраняется) + ДВОЙНОЕ нажатие Esc очищает экран.

  На CardKB физически нет клавиши Ctrl (есть только Esc/Tab/Sym),
  поэтому вместо Ctrl+L используется двойное нажатие Esc подряд:
    - Esc один раз (и пауза ~400 мс без повторного нажатия)
      -> включить/выключить экран
    - Esc два раза подряд быстро (в пределах ~400 мс)
      -> очистить экран
  Это работает на одной-единственной клавише Esc, без модификаторов.

  ВАЖНО про Esc-выключение:
  Пин LED дисплея у вас подключён напрямую к 3.3V, поэтому программно
  погасить физическую подсветку нельзя. Esc делает "программное"
  выключение: экран чернеет, а повторный Esc восстанавливает текст
  из буфера в памяти (screenBuffer). Если нужно по-настоящему гасить
  подсветку - провод LED нужно перекинуть на свободный GPIO.

  ВАЖНО про коды клавиш:
  Коды стрелок (0xB4-0xB7) соответствуют типичной прошивке CardKB
  v1.1, но могут отличаться у вашего экземпляра. Если что-то не
  сработает - откройте Serial Monitor (115200 бод), нажмите клавишу
  и посмотрите строку "Неизвестный код: 0x..", затем поправьте
  соответствующий #define ниже.

  Версия 5: добавлена ТАБЛИЦА КОМБИНАЦИЙ fn+буква.

  ВАЖНО: на плате ДВЕ РАЗНЫЕ клавиши-модификатора, это не одно и то же:
    - Sym (внизу слева, рядом с Z) - её CardKB обрабатывает САМА и сразу
      шлёт готовый печатаемый ASCII-символ (например, Sym+1 -> '!',
      Sym+',' -> '<', Sym+Q -> '{' - значки видно прямо на клавиатуре
      мелким шрифтом над буквами). Эти символы УЖЕ попадают в диапазон
      0x20-0x7E и автоматически печатаются через typeChar() - для них
      ничего настраивать не нужно, они и так работают "из коробки".
    - fn (вверху справа, рядом с P) - вот это и есть настоящий модификатор
      для СВОИХ комбинаций. Зажатая fn+буква присылает отдельный код,
      который нужно один раз откалибровать через Serial Monitor (там
      будет видно "Неизвестный код: 0x.." - это и есть нужное значение)
      и вписать в таблицу fnBindings[] ниже. Новую комбинацию добавляете
      одной строкой, без правки остальной логики.

  Версия 6: добавлена ОБОБЩЁННАЯ система комбинаций из ДВУХ ОБЫЧНЫХ
  клавиш подряд (без fn) - например Enter+R, Tab+C, Sym+Enter и т.п.

  В отличие от fn-комбинаций (где клавиатура сама склеивает нажатия в
  один байт), здесь комбинация - это последовательность во времени:
  первая клавиша придерживается на seqWindow миллисекунд, и если
  вторая нужная клавиша прилетела вовремя - срабатывает комбинация,
  если нет - выполняется обычное (одиночное) действие первой клавиши.
  Это ровно тот же приём, что раньше был "зашит" только под двойной
  Esc, но теперь обобщён на любую пару клавиш через таблицу seqCombos[].

  Задержка перед одиночным действием появляется ТОЛЬКО у тех клавиш,
  которые реально участвуют в какой-то комбинации как "первая" (если
  таблица пустая - никаких задержек нигде не будет).

  Версия 7: Enter теперь ОДИН сразу вызывает submitQuest() (без
  задержки), а Sym+Enter делает перенос строки (newLine()). Для этого
  из таблицы seqCombos[] убран старый тестовый пример "Enter+R" - он
  делал KEY_ENTER "первой клавишей" комбинации, из-за чего ЛЮБОЙ Enter
  на 400 мс зависал в ожидании второй клавиши, прежде чем сработать.
  Теперь KEY_ENTER используется только как ВТОРАЯ клавиша (в Sym+Enter),
  поэтому одиночный Enter срабатывает мгновенно, без задержек.

  Версия 8: ВЫЯСНИЛОСЬ, что Sym+Enter физически невозможен. Sym - это
  не полноценный модификатор вроде fn, а просто подсказка прошивке,
  какой альтернативный символ напечатать для КОНКРЕТНОЙ клавиши (и
  только для тех клавиш, у которых на корпусе нарисован второй мелкий
  символ - цифры, запятая и т.п.). У Enter такого символа нет, поэтому
  Sym+Enter и просто Enter шлют ОДИН И ТОТ ЖЕ байт 0x0D - разделить их
  программно нельзя, разница физически отсутствует.

  Вместо этого перенос строки повешен на fn+Enter: fn - настоящий
  аппаратный модификатор (как и для fn+буква), который даёт отдельный
  код для комбинации. Поэтому перенос строки реализован через обычную
  таблицу fnBindings[] (без таймера/ожидания - срабатывает мгновенно).

  ВАЖНО: FN_ENTER (код комбинации fn+Enter) - пока ЗАГЛУШКА, нужно
  откалибровать через Serial Monitor (зажмите fn и нажмите Enter -
  увидите "Неизвестный код: 0x..", это и есть нужное значение).

  Библиотеки: Adafruit GFX Library, Adafruit ILI9341, Adafruit BusIO
*/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// ---------- Пины дисплея (SPI) ----------
#define TFT_CS    7
#define TFT_RST   5
#define TFT_DC    6
#define TFT_MOSI  10
#define TFT_SCK   9
#define TFT_MISO  8

// ---------- Пины CardKB (I2C) ----------
#define I2C_SDA   1
#define I2C_SCL   2
#define CARDKB_I2C_ADDR 0x5F

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST, TFT_MISO);

// ---------- Коды спецклавиш CardKB ----------
#define KEY_BACKSPACE 0x08
#define KEY_ENTER     0x0D
#define KEY_TAB       0x09
#define KEY_ESC       0x1B
#define KEY_LEFT      0xB4
#define KEY_UP        0xB5
#define KEY_DOWN      0xB6
#define KEY_RIGHT     0xB7



// ---------- Параметры текстовой сетки ----------
#define TEXT_SIZE     2
#define MARGIN_X      5
#define MARGIN_Y      5
#define MAX_COLS      40   // запас с большим запасом под любой размер шрифта
#define MAX_ROWS      20

const int charW = 6 * TEXT_SIZE;
const int charH = 8 * TEXT_SIZE;

int screenW, screenH;
int maxCols, maxRows;

int col = 0, row = 0;              // позиция курсора
char screenBuffer[MAX_ROWS][MAX_COLS]; // содержимое экрана (для Esc и курсора)

// ---------- Таблица комбинаций fn + буква ----------
// (fn - отдельная клавиша вверху справа платы, рядом с P. Это НЕ Sym!)
//
// Как добавить новую комбинацию:
//   1) Зажмите fn и нужную букву, посмотрите код в Serial Monitor
//      (он покажется как "Неизвестный код: 0x..", пока комбинация
//      не добавлена в таблицу ниже - это и есть нужное значение).
//   2) Впишите код в #define (для читаемости, необязательно).
//   3) Напишите отдельную функцию-обработчик (без аргументов).
//   4) Добавьте одну строку в массив fnBindings[] ниже.
// Никакие другие места в коде трогать не нужно.

// ЗАГЛУШКИ - это НЕ настоящие коды, их нужно заменить на значения,
// которые вы увидите в Serial Monitor при нажатии fn+H и fn+T!
#define FN_H 0x9F      // fn + H (пример: "поздороваться") - ЗАМЕНИТЬ
#define FN_T 0x91      // fn + T (пример: вставить метку времени) - ЗАМЕНИТЬ
#define FN_ENTER 0xA3  // fn + Enter -> перенос строки - ОБЯЗАТЕЛЬНО ЗАМЕНИТЬ!

// ---- Функции-обработчики комбинаций (пишете свою логику сюда) ----
void fnSayHello() {
  Serial.println("fn+H сработал: привет!");
  // например, можно вывести текст прямо на экран:
  // printResponseToScreen("Privet!\n");
}

void fnInsertTimestamp() {
  Serial.println("fn+T сработал: здесь можно вставить что угодно");
  // пример: напечатать на экране текущее значение millis()
  // String ts = String(millis());
  // for (int i = 0; i < ts.length(); i++) typeChar(ts[i]);
}

struct FnBinding {
  uint8_t code;
  void (*action)();
  const char* description;
};

// Сама таблица - сюда добавляете новые строки для новых комбинаций
FnBinding fnBindings[] = {
  { FN_H,     fnSayHello,        "fn+H - тестовое приветствие" },
  { FN_T,     fnInsertTimestamp, "fn+T - заглушка под свою функцию" },
  { FN_ENTER, newLine,           "fn+Enter -> перенос строки" },
};
const int fnBindingsCount = sizeof(fnBindings) / sizeof(fnBindings[0]);

uint8_t lastFnCode = 0; // чтобы комбинация срабатывала один раз за нажатие,
                         // а не повторялась всё время, пока клавиши зажаты

// Ищет код в таблице и запускает нужную функцию. Возвращает true, если
// код был найден и обработан (значит это не "неизвестная" клавиша).
bool tryFnBinding(uint8_t code) {
  for (int i = 0; i < fnBindingsCount; i++) {
    if (fnBindings[i].code == code) {
      if (code != lastFnCode) {
        fnBindings[i].action();
      }
      lastFnCode = code;
      return true;
    }
  }
  return false;
}

bool displayOn = true;
bool cursorVisible = false;
unsigned long lastBlink = 0;
const unsigned long blinkInterval = 500; // мс

// ---------- Состояние для двойного нажатия Esc ----------
bool escPending = false;
unsigned long escPendingTime = 0;
const unsigned long doubleTapWindow = 400; // мс - сколько ждать второй Esc

// ---------- Таблица комбинаций из ДВУХ ОБЫЧНЫХ клавиш подряд ----------
// (например Tab+C - без fn)
//
// ВАЖНО: годится только для пар из клавиш, которые физически шлют РАЗНЫЕ
// коды сами по себе (как Tab и C). Если одна из клавиш (как Sym для
// Enter) не имеет собственного кода и не меняет код второй клавиши -
// такую пару эта таблица в принципе не различит, там нужен fn (см. выше).
//
// Как добавить новую комбинацию:
//   1) Определите код первой и второй клавиши (обычные KEY_.. define
//      либо буква вроде 'c').
//   2) Напишите функцию-обработчик без аргументов.
//   3) Добавьте строку в массив seqCombos[] ниже: { перваяКлавиша,
//      втораяКлавиша, функция, "описание" }.
// Больше ничего менять не нужно - движок работает для всех строк
// таблицы автоматически.

void seqTabC() {
  Serial.println("Tab+C сработал: очищаю экран (пример)");
  clearBuffer();
  if (displayOn) fullRedraw();
}

struct SeqCombo {
  uint8_t firstKey;
  uint8_t secondKey;
  void (*action)();
  const char* description;
};

SeqCombo seqCombos[] = {
  { KEY_TAB, 'c', seqTabC, "Tab+C - пример" },
  // Sym+Enter сюда добавить нельзя - Sym не даёт своего отдельного кода
  // для клавиш без alt-символа на корпусе (см. комментарий в начале файла).
  // Перенос строки по модификатору реализован через fn+Enter (см. fnBindings[] выше).
};
const int seqCombosCount = sizeof(seqCombos) / sizeof(seqCombos[0]);

bool seqPending = false;
uint8_t seqFirstKey = 0;
unsigned long seqPendingTime = 0;
const unsigned long seqWindow = 400; // мс - сколько ждать вторую клавишу

// Есть ли в таблице хоть одна комбинация, где code - первая клавиша?
bool isSeqFirstKey(uint8_t code) {
  for (int i = 0; i < seqCombosCount; i++) {
    if (seqCombos[i].firstKey == code) return true;
  }
  return false;
}

// Ищет комбинацию (first, second) в таблице; если нашлась - выполняет
// и возвращает true
bool trySeqCombo(uint8_t first, uint8_t second) {
  for (int i = 0; i < seqCombosCount; i++) {
    if (seqCombos[i].firstKey == first && seqCombos[i].secondKey == second) {
      seqCombos[i].action();
      return true;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  tft.begin();
  tft.setSPISpeed(40000000);
  tft.setRotation(1);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(TEXT_SIZE);
  tft.setTextWrap(false);

  screenW = tft.width();
  screenH = tft.height();
  maxCols = min((screenW - MARGIN_X) / charW, MAX_COLS);
  maxRows = min((screenH - MARGIN_Y) / charH, MAX_ROWS);

  clearBuffer();
  fullRedraw();

  Serial.println("CardKB + TFT готовы (v8: Enter=отправка, fn+Enter=перенос строки, Tab+C, fn+буква)");
}

void loop() {
  char key = readCardKB();
  if (key != 0) {
    handleKey(key);
  } else {
    lastFnCode = 0; // клавиша отпущена - разрешаем повторный запуск Fn-комбинации
  }

  if (displayOn) {
    updateCursorBlink();
  }

  checkEscTimeout();
  checkSeqTimeout();

  delay(5);
}

char readCardKB() {
  Wire.requestFrom(CARDKB_I2C_ADDR, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0;
}

void handleKey(char cRaw) {
  uint8_t c = (uint8_t)cRaw;

  // Esc - отдельная, независимая логика (двойное/одиночное нажатие),
  // всегда, даже если экран "выключен"
  if (c == KEY_ESC) {
    handleEscKey();
    return;
  }

  // Если экран выключен - игнорируем остальные клавиши
  if (!displayOn) {
    return;
  }

  // --- Есть "зависшая" первая клавиша предыдущей seq-комбинации? ---
  if (seqPending) {
    uint8_t firstKeyBackup = seqFirstKey;
    seqPending = false;

    if (trySeqCombo(firstKeyBackup, c)) {
      return; // комбинация сработала, вторую клавишу отдельно не обрабатываем
    } else {
      // не совпало - выполняем обычное действие первой клавиши,
      // а текущую клавишу обрабатываем заново, уже как новую "первую"
      dispatchStandaloneKey(firstKeyBackup);
      handleKey((char)c);
      return;
    }
  }

  // --- Эта клавиша - "первая" в какой-то из комбинаций таблицы? ---
  if (isSeqFirstKey(c)) {
    seqPending = true;
    seqFirstKey = c;
    seqPendingTime = millis();
    return; // ждём вторую клавишу, пока не действуем
  }

  // --- Обычная обработка (без задержек) ---
  dispatchStandaloneKey(c);
}

// То, что раньше было прямо в switch внутри handleKey - вынесено сюда,
// чтобы можно было вызывать "обычное" действие клавиши и из seq-движка
void dispatchStandaloneKey(uint8_t c) {
  switch (c) {
    case KEY_BACKSPACE:
      backspace();
      break;
    case KEY_ENTER:
      submitQuest(); // одиночный Enter - "отправка запроса", НЕ перенос строки
      break;
    case KEY_LEFT:
      moveCursor(-1, 0);
      break;
    case KEY_RIGHT:
      moveCursor(1, 0);
      break;
    case KEY_UP:
      moveCursor(0, -1);
      break;
    case KEY_DOWN:
      moveCursor(0, 1);
      break;
    default:
      if (c >= 0x20 && c <= 0x7E) {
        typeChar(c);
      } else if (tryFnBinding(c)) {
        // код найден в таблице fnBindings[] и уже обработан
      } else {
        // Неопознанная клавиша - печатаем код в Serial для калибровки
        Serial.print("Неизвестный код: 0x");
        Serial.println(c, HEX);
      }
      break;
  }
}

// ---------- Низкоуровневая отрисовка одной ячейки ----------
void drawCell(int c, int r, char ch, bool inverted) {
  int x = MARGIN_X + c * charW;
  int y = MARGIN_Y + r * charH;

  if (inverted) {
    tft.fillRect(x, y, charW, charH, ILI9341_WHITE);
    tft.setTextColor(ILI9341_BLACK, ILI9341_WHITE);
  } else {
    tft.fillRect(x, y, charW, charH, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  }

  if (ch != ' ' && ch != 0) {
    tft.setCursor(x, y);
    tft.print(ch);
  }

  // возвращаем цвет по умолчанию
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
}

// ---------- Буфер ----------
void clearBuffer() {
  for (int r = 0; r < MAX_ROWS; r++) {
    for (int c = 0; c < MAX_COLS; c++) {
      screenBuffer[r][c] = ' ';
    }
  }
  col = 0;
  row = 0;
}

void fullRedraw() {
  tft.fillScreen(ILI9341_BLACK);
  for (int r = 0; r < maxRows; r++) {
    for (int c = 0; c < maxCols; c++) {
      if (screenBuffer[r][c] != ' ') {
        drawCell(c, r, screenBuffer[r][c], false);
      }
    }
  }
  cursorVisible = false;
  drawCursor(true);
}

// ---------- Ввод текста ----------
void typeChar(char c) {
  screenBuffer[row][col] = c;
  drawCell(col, row, c, false);
  advanceCursor();
}

void advanceCursor() {
  eraseCursorHighlight();
  col++;
  if (col >= maxCols) {
    col = 0;
    row++;
  }
  if (row >= maxRows) {
    // экран заполнен - начинаем перезаписывать с начала (как кольцевой буфер)
    row = 0;
  }
  drawCursor(true);
}

void newLine() {
  eraseCursorHighlight();
  col = 0;
  row++;
  if (row >= maxRows) row = 0;
  drawCursor(true);
}

// Вызывается по ОДИНОЧНОМУ Enter (без Sym). Сюда позже подключите
// отправку накопленной currentLine на сервер/ИИ (см. cardkb_guide.md,
// раздел 8 про currentLine) - пока просто лог в Serial для проверки.
void submitQuest() {
  Serial.println("Отправлен вопрос");
  // пример на будущее:
  // onCommandReady(currentLine);
  // currentLine = "";
}

void backspace() {
  eraseCursorHighlight();
  if (col > 0) {
    col--;
  } else if (row > 0) {
    row--;
    col = maxCols - 1;
  }
  screenBuffer[row][col] = ' ';
  drawCell(col, row, ' ', false);
  drawCursor(true);
}

// ---------- Курсор ----------
void moveCursor(int dCol, int dRow) {
  eraseCursorHighlight();

  int newCol = col + dCol;
  int newRow = row + dRow;

  if (newCol < 0) newCol = 0;
  if (newCol >= maxCols) newCol = maxCols - 1;
  if (newRow < 0) newRow = 0;
  if (newRow >= maxRows) newRow = maxRows - 1;

  col = newCol;
  row = newRow;

  drawCursor(true);
}

// рисует/прячет подсветку текущей ячейки курсора
void drawCursor(bool visible) {
  drawCell(col, row, screenBuffer[row][col], visible);
  cursorVisible = visible;
  lastBlink = millis();
}

// убирает подсветку курсора (возвращает обычный вид символа)
void eraseCursorHighlight() {
  drawCell(col, row, screenBuffer[row][col], false);
  cursorVisible = false;
}

void updateCursorBlink() {
  if (millis() - lastBlink >= blinkInterval) {
    cursorVisible = !cursorVisible;
    drawCell(col, row, screenBuffer[row][col], cursorVisible);
    lastBlink = millis();
  }
}

// ---------- Esc: одно нажатие = вкл/выкл, двойное = очистка ----------
void handleEscKey() {
  if (escPending && (millis() - escPendingTime <= doubleTapWindow)) {
    // Это второй Esc в пределах окна -> очистка экрана
    escPending = false;
    clearBuffer();
    if (displayOn) {
      fullRedraw();
    }
    // если экран был выключен - буфер очищен, но пиксели остаются чёрными,
    // следующее одиночное Esc включит уже пустой экран
  } else {
    // Это первый Esc - ждём, не будет ли второго
    escPending = true;
    escPendingTime = millis();
  }
}

// вызывается каждую итерацию loop(): если второй Esc не пришёл вовремя,
// считаем нажатие одиночным и переключаем экран
void checkEscTimeout() {
  if (escPending && (millis() - escPendingTime > doubleTapWindow)) {
    escPending = false;
    toggleDisplay();
  }
}

// вызывается каждую итерацию loop(): если вторая клавиша seq-комбинации
// не пришла вовремя, выполняем обычное (одиночное) действие первой
void checkSeqTimeout() {
  if (seqPending && (millis() - seqPendingTime > seqWindow)) {
    seqPending = false;
    dispatchStandaloneKey(seqFirstKey);
  }
}

void toggleDisplay() {
  displayOn = !displayOn;
  if (!displayOn) {
    tft.fillScreen(ILI9341_BLACK); // "гасим" пиксели, буфер не трогаем
  } else {
    fullRedraw(); // восстанавливаем содержимое из буфера
  }
}