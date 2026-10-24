# Node-ID CANopen-сервера: значение вместо параметра шаблона — план

Статус: emblib и etk сделаны 23 октября 2026 года (emblib c8dc7e4, e0ac50e,
ee819d3; etk 48cf642, 2cc6f62), все четыре пресета собираются без
предупреждений. На стенде не проверялось, остальные потребители не
переведены (раздел 4). Измеренные размеры — в разделе 7, отличия
реализации от плана — в разделе 8.

Конструкции раздела 2 проверены прототипом в копии дерева на
arm-none-eabi-g++ 15.2 с флагами прошивки, включая `-Werror`. emblib с
изменением собрана вместе с etk в двух вариантах:
- шаг 2 — три TU, которые доходят до сервера (`od.cpp`, `server.cpp`,
  `main.cpp`), на rev-a-release и rev-a-debug;
- шаг 4 — все шесть TU, которые доходят до изменённых заголовков, на всех
  четырёх пресетах: Node-ID на 3100h, остальные параметры сдвинуты на 20h.

Все TU emblib под `emb/can`, включая новый тест, собраны на rev-a-release и
rev-a-debug. Тест и проба по образцу h2-hess собраны также host g++ 16.2 и
clang 22 через `clang-check`. Двенадцать негативных проб дают ошибки из
раздела 5. Текст ошибки для примера §5.5 описания алгоритма (шаг 3) взят
из пробы с новой раскладкой.

Что меняется. Node-ID перестаёт быть параметром шаблона `server` и шести
классов `detail`. Он приходит в конструктор сервера значением типа
`node_id`, а COB-ID служб считаются из него при создании. Число PDO и
ёмкость очереди остаются в `server_options`, потому что задают размеры
массивов. API emblib ломается. При том же Node-ID на шине ничего не
меняется. Затем etk берёт Node-ID из настройки `canopen.node_id`, которая
вступает в силу после перезапуска. Её запись — 3100h, первая среди
параметров; остальные параметры сдвигаются на блок 20h.

## Решения

- 2026-10-23. Node-ID — значение типа `node_id`, аргумент конструктора
  `server`. В `server_options` остаются `tpdo_count`, `rpdo_count` и
  `rx_queue_capacity`: они задают размеры массивов. Шаблон давал две вещи.
  Первая — проверка диапазона при компиляции; её сохраняет конструктор
  `consteval`, `node_id{1}`. Вторая — COB-ID как непосредственные
  операнды, то есть пара тактов на кадр и десяток байт. Отвергнутый
  вариант — оставить шаблон
  и задавать Node-ID пресетом сборки. Тогда на каждый адрес нужен свой
  образ, а хосты уже берут адрес снаружи (раздел 1).
- 2026-10-23. Константа становится `node_id` через явный конструктор
  `consteval`, `node_id{1}`, а значение времени выполнения — через `make`,
  который возвращает `std::optional`. Так же разделены `std::format_string`,
  чей конструктор `consteval` проверяет строку формата, и
  `std::runtime_format`. Диапазон проверяет только `make`, конструктор
  зовёт его, так что отдельного предиката нет. Открыты `min` и `max`: по ним
  задаются границы настройки. Конструктор отказывает так же, как `param` в
  настройках: через `EMB_PANIC` (`emb/panic.hpp`), чью причину диагностика
  показывает в строке ошибки; `throw` в consteval под `-fno-exceptions` не
  компилируется (§8 плана словаря). Отвергнутые варианты:
  - фабрика `literal(v)`, как в cannet: имя говорит, как выглядит
    аргумент, а нужна константа — `literal(base + 2)` тоже работает;
  - `literal<V>()` со `static_assert`;
  - открытый предикат, как `is_valid` в cannet: на тот же вопрос отвечает
    `make(v)`.
- 2026-10-23. В `cob_id_of` служба и номер PDO остаются аргументами
  шаблона, а Node-ID становится аргументом функции. Функции constexpr,
  поэтому контракт h2-hess по-прежнему вычисляется при компиляции.
  Отвергнутый вариант — всё значениями, как `cob_id` и `pdo_cob_id` в
  cannet. Тогда номер PDO и класс службы проверялись бы только во время
  выполнения, а на устройстве они всегда константы.
- 2026-10-23. Node-ID читается один раз, при запуске. Новый Node-ID
  вступает в силу после перезапуска (`apply::on_restart`). Отвергнутый
  вариант — смена по NMT reset communication, как в CiA 301. У `transport`
  нет снятия фильтров (`detail/rpdo_consumer.hpp:46-47`), а
  `on_reset_communication` в etk ни к чему не подключён.
- 2026-10-23. `watch_heartbeat` и `hb_consumer::watch` тоже берут
  удалённый Node-ID значением, а `on_lost` получает `node_id`. Их не
  вызывает ни один потребитель. Параметром шаблона `node_id` быть не может:
  это не структурный тип (раздел 6).
- 2026-10-23. Аксессора Node-ID у сервера нет. У сервера до 8aedb30 был
  `id()`, но сейчас он никому не нужен.
- 2026-10-23. Остальные потребители переходят при своём следующем бампе
  emblib (раздел 4), как в плане словаря.
- 2026-10-23. Настройка входит в серию. Node-ID — первый параметр, запись
  3100h, в блоке 3100h–311Fh группы `canopen`. Остальные 67 записей
  сдвигаются на блок 20h, с 3120h по 3242h. Словарь etk пока никто не видел
  и не использует, так что адреса можно двигать. Сохранённые значения не
  теряются: идентификатор значения выведен из имени и типа параметра
  (`emb/settings/param.hpp:293`), а не из индекса. Отвергнутый вариант —
  3240h, первый свободный блок: адреса остались бы на месте, но Node-ID
  стоял бы последним.
- 2026-10-23. Группа настройки — `canopen`, путь `canopen.node_id`.
  Node-ID — понятие CANopen, в кадре CAN адреса узла нет. Вероятные соседи
  по группе — период heartbeat, SYNC, периоды PDO, таймауты RPDO — тоже из
  CANopen, а скорость шины CANopen настраивает вместе с Node-ID (LSS,
  CiA 305). Имя совпадает с `settings::canopen`, `comm::can::canopen` и
  `emb::can::canopen`. Отвергнутые варианты — `can`, неточное для Node-ID,
  и `comm`. Общее имя понадобилось бы только со вторым интерфейсом, но
  тогда `comm.node_id` не говорит, на какой шине этот адрес, а путь из двух
  частей не даёт написать `comm.canopen.node_id`. Имя входит в
  идентификатор сохранённого значения (`emb/settings/param.hpp:53-59`), так
  что переименование потом стоило бы сохранённого Node-ID: узел встал бы
  на 1.

## 1. Что сейчас

- Node-ID стал параметром шаблона в emblib 8aedb30 (15 июня 2026,
  «compile-time node id + arbitrary TPDO/RPDO count») вместе с числом PDO.
  Числу PDO параметр шаблона нужен: оно задаёт `std::array` слотов. Node-ID
  ушёл туда заодно. Класс `node_id` с `make` удалили в пользу концепта
  `valid_node_id`, а COB-ID служб стали `static constexpr`. Сообщение
  коммита объясняет это тем, что все COB-ID вычисляются при компиляции, а
  Node-ID проверяет `static_assert`.
- `server_options` (`server.hpp:29-34`) — это `{node_id, tpdo_count,
  rpdo_count, rx_queue_capacity}`, параметр шаблона `server<Opt, Ctx>`. На
  строке 38 стоит `static_assert(valid_node_id<Opt.node_id>)`. От Node-ID
  параметризованы шесть классов `detail`.

| Класс | Что делает с Node-ID |
|---|---|
| `nmt_slave<NodeId>` | сравнивает с адресатом команды NMT (`nmt_slave.hpp:44`) |
| `hb_producer<NodeId>` | COB-ID heartbeat в `tick` (`hb_producer.hpp:34`) |
| `emcy_producer<NodeId>` | COB-ID EMCY в `emit` (`emcy_producer.hpp:27`) |
| `sdo_server<NodeId, Ctx>` | `static constexpr` COB-ID RSDO и TSDO (`sdo_server.hpp:82-83`), фильтр RSDO в конструкторе |
| `tpdo_producer<NodeId, N>` | COB-ID PDO 1–4 из предопределённого набора в `setup` (`tpdo_producer.hpp:35`) |
| `rpdo_consumer<NodeId, N>` | то же, плюс фильтр (`rpdo_consumer.hpp:43`) |

- `cob_id_of` (`types.hpp:86-110`) — три перегрузки: широковещательные
  `<Service>()`, адресные `<Service, NodeId>()` и PDO `<Pdo, I, NodeId>()`.
- `hb_consumer::watch<Remote>` и `server::watch_heartbeat<Remote>` берут
  удалённый Node-ID параметром шаблона; их не вызывает ни один потребитель.
  `server::options()` (`server.hpp:71`) тоже никто не вызывает, он остаётся.
- etk: `.node_id = 0x01` стоит в `settings::canopen::server_options`
  (`settings/canopen.hpp:13-17`). Член `server_` объявлен в
  `comm/can/canopen/server.hpp:65-66`, конструктор обёртки — в
  `server.cpp:28-36`. Настройки загружаются раньше, чем создаётся сервер
  (`main.cpp:94` и `202`). Всё, что нужно настройке с применением после
  перезапуска, уже есть: политика `apply::on_restart` (так применяются
  `motor.pole_pairs` и `hall.enabled`), команда 2101h:02 «сохранить и
  перезапустить», флаг 3000h:11 «требуется перезапуск».
- Параметр настроек в словаре — запись (`od_settings.hpp:242-255`). Её
  строка в `od.cpp` стоит на 00h, но 00h — наибольший подындекс, 09h.
  Запрошенное значение — 01h, применённое — 02h, сохранённое — 03h,
  политика — 07h; 08h применяет параметр, 09h отбрасывает запрос. 67
  записей etk лежат блоками по 20h: drive 3100h, protection 3120h, motor
  3140h, model 3160h, controllers 3180h, mras 31A0h, current_sensor 31C0h,
  hall 31E0h, hall_calibration 3200h, temperature_sensor 3220h.
- Примеры в `canopen-od-algorithm.ru.md` и `settings-nvm-design.md` взяты
  из словаря etk и называют его индексы. Когда etk переносил отпечаток
  словаря (ed02775), emblib перед этим отдельным коммитом (fd4b9c7)
  записала решение и таблицу «было → стало» для оператора и обновила
  примеры, а etk повторил адреса в теле своего коммита.
- Хосты уже берут Node-ID снаружи. Демон workbench — ключом `--node`
  (`daemon/src/main.cpp:116-123`), ucan-monitor — через `NodeId` сервера и
  `update_node_id()`. В cannet есть тип `node_id` с `make` и `literal`
  (`canopen/include/canopen/types.hpp:50-90`).
- В sevpress, который сидит на emblib до 8aedb30, Node-ID — настройка:
  параметр `ucanopen.node_id` (uint32, 3000h:01, rw). При запуске сервер
  получает (`src/comm/can/canopen/server.cpp:30-33`)

  ```cpp
  node_id::make(static_cast<std::uint8_t>(cfg.node_id)).value()
  ```

  Приведение к `uint8_t` стоит до проверки и пропускает, например, 261 как
  5.

## 2. Дизайн

### 2.1 `node_id` (`types.hpp`)

```cpp
// The class `node_id` holds the id of a CANopen node, from 1 to 127. A
// constant constructs one, checked at compile time, and `make` a value known
// at run time, so an id outside the range never reaches the arithmetic of
// COB-IDs.
class node_id {
public:
  static constexpr unsigned min = 1;
  static constexpr unsigned max = 127;

  // Constructs the id `value`. Compilation fails here if `value` is outside
  // [1, 127]; a value known only at run time goes through `make`.
  explicit consteval node_id(unsigned value)
      : value_(static_cast<std::uint8_t>(value))
  {
    if (!make(value)) EMB_PANIC("node_id: the id must be in [1, 127]");
  }

  // Returns the id `value`, or `std::nullopt` if `value` is outside
  // [1, 127].
  static constexpr std::optional<node_id> make(unsigned value)
  {
    if (value < min || value > max) return std::nullopt;
    return node_id(unchecked{}, static_cast<std::uint8_t>(value));
  }

  constexpr std::uint8_t get() const
  {
    return value_;
  }

  friend constexpr bool operator==(node_id, node_id) = default;

private:
  struct unchecked {};

  constexpr node_id(unchecked, std::uint8_t value) : value_(value) {}

  std::uint8_t value_;
};
```

Диапазон проверяет только `make`; конструктор зовёт его. `make` проверяет
значение до сужения до `uint8_t`, поэтому `make(261)` даёт `nullopt`, а не
5. Конструктор сужает раньше проверки, но для значения вне диапазона
вычисление обрывается на `EMB_PANIC`, и объекта нет:
`node_id{300}` не компилируется, а не даёт 44.

Конструктор `explicit`, поэтому `node_id x = 1;` и `f(1)` не компилируются.
Переменную он не примет: `node_id{v}` — ошибка компиляции. Закрытый
конструктор с меткой `unchecked` нужен `make`, потому что открытый —
`consteval`. Конструктора по умолчанию нет. `sizeof(node_id) == 1`, тип
тривиально копируемый. Концепт `valid_node_id` уходит. В `types.hpp`
добавляется `<optional>`.

### 2.2 `cob_id_of` (`types.hpp`)

Широковещательная перегрузка `<Service>()` не меняется. Две другие
выглядят так:

```cpp
template<cob_type Service>
  requires peer_cob<Service>
constexpr id_t cob_id_of(node_id node)
{
  return cob_function_codes[std::to_underlying(Service)] + node.get();
}

template<cob_type Pdo, std::size_t I>
  requires pdo_cob<Pdo>
constexpr id_t cob_id_of(node_id node)
{
  static_assert(I >= 1 && I <= 4, "predefined COB-ID exists only for PDO 1..4");
  return cob_function_codes[std::to_underlying(Pdo)]
       + static_cast<id_t>(0x100 * (I - 1))
       + node.get();
}
```

Перегрузки различаются числом аргументов шаблона и функции. Поэтому для
`cob_id_of<cob_type::heartbeat>()` и `cob_id_of<cob_type::tpdo>(n)`
перегрузки не находится; тест проверяет это концептами (раздел 5). Функции
constexpr, и COB-ID контракта h2-hess годятся в аргументы шаблона, как в
`setup_tpdo<1, pdo_id::custom(chss::rpdo1_cob_id)>`.

### 2.3 Классы `detail`

| Было | Стало | Что хранит |
|---|---|---|
| `nmt_slave<NodeId>` | `nmt_slave(node_id)` | `node_id`; методы constexpr — ради теста |
| `hb_producer<NodeId>(bus)` | `hb_producer(bus, node_id)` | `id_t cob_id_` = 700h + id |
| `emcy_producer<NodeId>(bus)` | `emcy_producer(bus, node_id)` | `id_t cob_id_` = 80h + id |
| `sdo_server<NodeId, Ctx>(bus, od_view, ctx)` | `sdo_server<Ctx>(bus, node_id, od_view, ctx)` | `rsdo_cob_id_`, `tsdo_cob_id_` |
| `tpdo_producer<NodeId, N>(bus)` | `tpdo_producer<N>(bus, node_id)` | `node_id`; `setup` зовёт `cob_id_of<cob_type::tpdo, I>(node_)` |
| `rpdo_consumer<NodeId, N>(bus)` | `rpdo_consumer<N>(bus, node_id)` | то же |
| `hb_consumer::watch<Remote>(timeout, on_lost, now)` | `watch(node_id remote, timeout, on_lost, now)` | `node_id remote` в слоте; `on_lost` — `delegate<void(node_id)>` |

COB-ID, на которые класс шлёт или которые слушает всегда, считаются в
конструкторе. PDO хранят сам Node-ID, потому что предопределённый COB-ID
выбирается в `setup<I, CobId>`.

В `sdo_server` члены `rsdo_cob_id_` и `tsdo_cob_id_` объявляются сразу за
`bus_`. Члены инициализируются в порядке объявления (`-Wreorder`), а фильтр
RSDO ставит тело конструктора. Удалённая перегрузка с `Ctx&&` тоже
получает `node_id`.

У `node_id` нет конструктора по умолчанию, но слоту `hb_consumer` это не
мешает: `inplace_vector` хранит элементы в union и по умолчанию их не
создаёт. `hb_consumer.cpp` не меняется: `w.on_lost(w.remote)` компилируется
как есть.

### 2.4 Сервер (`server.hpp`)

```cpp
struct server_options {
  std::size_t tpdo_count = 4;
  std::size_t rpdo_count = 4;
  std::size_t rx_queue_capacity = 32;
};

template<server_options Opt, typename Ctx>
class server {
public:
  server(emb::delegate<std::chrono::milliseconds()> clock,
         transport& bus,
         node_id node,
         od_view<Ctx> dictionary,
         Ctx& ctx);
  // nmt_(node), hb_producer_(bus, node), sync_producer_(bus),
  // hb_consumer_(bus), emcy_(bus, node), sdo_(bus, node, dictionary, ctx),
  // tpdo_(bus, node), rpdo_(bus, node)

  server(emb::delegate<std::chrono::milliseconds()> clock,
         transport& bus,
         node_id node,
         od_view<Ctx> dictionary,
         Ctx&& ctx) = delete;

  bool watch_heartbeat(node_id remote,
                       std::chrono::milliseconds timeout,
                       emb::delegate<void(node_id)> on_lost);

private:
  detail::nmt_slave nmt_;
  detail::hb_producer hb_producer_;
  detail::sync_producer sync_producer_;
  detail::hb_consumer hb_consumer_;
  detail::emcy_producer emcy_;
  detail::sdo_server<Ctx> sdo_;
  detail::tpdo_producer<Opt.tpdo_count> tpdo_;
  detail::rpdo_consumer<Opt.rpdo_count> rpdo_;
};
```

Аргументы идут в том же порядке, что у сервера до 8aedb30: часы, шина,
Node-ID, словарь. `static_assert` на Node-ID уходит: неверный `node_id`
построить нельзя. Опции по-прежнему пишутся на месте:
`server<{}, Ctx>` и `server<{.tpdo_count = 5, .rpdo_count = 0}, Ctx>`.

### 2.5 Шина

При том же Node-ID на шине ничего не меняется. COB-ID те же. Фильтры те же
и ставятся в прежнем порядке: RSDO — в конструкторе `sdo_server`, NMT — в
теле конструктора сервера, RPDO — в `setup_rpdo`. Адресат NMT разбирается
так же. Это верно для etk, bike и psfb (узел 1) и для узлов 1–5 h2-hess.

### 2.6 etk: константа (шаг 2)

```cpp
// settings/canopen.hpp
inline constexpr emb::can::canopen::server_options server_options{
    .tpdo_count = 4,
    .rpdo_count = 4,
    .rx_queue_capacity = 32};

inline constexpr emb::can::canopen::node_id node_id{0x01};
```

`comm/can/canopen/server.cpp` передаёт `settings::canopen::node_id` в
конструктор `server_` третьим аргументом.

### 2.7 etk: настройка (шаг 4)

```cpp
// settings/schema.hpp, после hall_calibration; подключает
// <emb/can/canopen/types.hpp>
namespace canopen {

inline constexpr auto node_id =
    param("canopen.node_id",
          std::uint32_t{1},
          {.title = {"Node ID", "Номер узла"},
           .min = std::uint32_t{emb::can::canopen::node_id::min},
           .max = std::uint32_t{emb::can::canopen::node_id::max},
           .apply = apply::on_restart});

} // namespace canopen
```

`params::canopen::node_id` идёт в `make_schema` последним, а
`schema.count` становится 68. Последним — чтобы не сдвинулись индексы
остальных параметров в схеме, то есть `arg` их записей; на них ссылаются
примеры §5.1 и §5.3 описания алгоритма («индекс 16», «индекс 17»).
Границы параметра взяты у `node_id`, так что разойтись они не могут.

```cpp
// settings/settings.cpp, рядом с read_hall_enabled; объявление — в
// settings.hpp
// The bounds of the parameter are those of a node id, and an image holds only
// values between the bounds, so make() always returns an id.
emb::can::canopen::node_id read_node_id(image const& from)
{
  auto const id =
      emb::can::canopen::node_id::make(from.get<params::canopen::node_id>());
  assert(id);
  return *id;
}
```

Образ держит только значения в границах параметра (`image.hpp:171-178`):
так проверяются и загрузка, и запись.

- `od.cpp`: 67 записей с 3100h по 3222h сдвигаются на 20h, и Node-ID
  встаёт на освободившуюся 3100h перед `drive.phase_swap`. Сдвиг делает
  скрипт, а не человек:

  ```
  perl -pi -e 's/^\{\{0x(3[12][0-9A-F]{2})(?=, 0x00\}, \{\},)/sprintf("{{0x%04X", hex($1) + (hex($1) >= 0x3100 && hex($1) <= 0x323F ? 0x20 : 0))/e' src/app/inverter/comm/can/canopen/od.cpp
  ```

  Он трогает только строки записей настроек: ключ с 00h и пустая метка.
  `git diff --word-diff` показывает 67 строк, и в каждой меняется только
  индекс; проба совпала с прототипом. Строку Node-ID и группу вставляют
  руками. `od_check` порядок групп не проверяет, но `groups` перечисляет их
  в порядке, в котором их впервые называют строки, поэтому `canopen` встаёт
  за `nvm`. Меняется отпечаток словаря (2000h:00).

  ```cpp
  {"nvm", {"Non-volatile memory", "Энергонезависимая память"}},
  {"canopen", {"CANopen", "CANopen"}},
  // ...
  {{0x3100, 0x00}, {}, setting::param<params::canopen::node_id>},

  {{0x3120, 0x00}, {}, setting::param<params::drive::phase_swap>},
  ```

- Конструктор `comm::can::canopen::server` получает
  `emb::can::canopen::node_id node` вторым аргументом и передаёт его
  `server_`. `main.cpp` передаёт `settings::read_node_id(applied)`.
- Константа `settings::canopen::node_id` из шага 2 уходит.
- Аппликатор не меняется. Параметры `on_restart` на ходу не применяются
  (`section.hpp:478`), а `rebuilds_nothing` смотрит только `on_command`
  (`settings.cpp:329-340`).

Записи сдвигаются так:

| Блок | Было | Стало |
|---|---|---|
| `canopen.node_id` | — | 3100h |
| drive, с `model.torque_slope` и `model.speed_slope` | 3100h–3107h | 3120h–3127h |
| protection | 3120h–3129h | 3140h–3149h |
| motor | 3140h–3144h | 3160h–3164h |
| model | 3160h–3167h | 3180h–3187h |
| controllers | 3180h–3187h | 31A0h–31A7h |
| mras | 31A0h–31A2h | 31C0h–31C2h |
| current_sensor | 31C0h–31C3h | 31E0h–31E3h |
| hall | 31E0h–31EDh | 3200h–320Dh |
| hall_calibration | 3200h–3203h | 3220h–3223h |
| temperature_sensor | 3220h–3222h | 3240h–3242h |

Чтобы сменить Node-ID, нужно записать 3100h:01, затем сохранить и
перезапустить (2101h:02). До перезапуска 3100h:02 остаётся прежним, а
3000h:11 читается как true. Команда «применить», 3100h:08, отвечает
06010000h: параметр применяется только перезапуском. Узел 1 возвращают
умолчания (2102h:01, затем 2101h:02), а также стирание параметров
(2102h:02) с перезапуском.

Первый запуск новой прошивки на устройстве с записью прежней загружает 67
значений на их новые места. Node-ID в записи нет (3000h:10 — 1), он берёт
умолчание 1, и до первого сохранения горит предупреждение
`params_defaulted` (`settings.cpp:408-410`).

## 3. План работ

Коммиты идут в обычном порядке: сначала emblib, затем etk с бампом emblib
в том же коммите.

0. **emblib** `docs(canopen): plan the node id at run time` — этот
   документ, как 927f8d9 для плана словаря.
1. **emblib** `feat(canopen)!: take the node id at run time`
   - изменить: `types.hpp`, `server.hpp`, `detail/nmt_slave.hpp`,
     `detail/hb_producer.hpp`, `detail/emcy_producer.hpp`,
     `detail/sdo_server.hpp`, `detail/tpdo_producer.hpp`,
     `detail/rpdo_consumer.hpp`, `detail/hb_consumer.hpp`;
   - создать: `emb/can/canopen/test/types_test.cpp`;
   - `docs/canopen-od-algorithm.ru.md`:
     - в §1.8 `detail::sdo_server<NodeId, Ctx>` становится
       `detail::sdo_server<Ctx>`, к тому, что он хранит, добавляются COB-ID
       RSDO и TSDO, а сервер передаёт ему ещё и Node-ID;
     - в §4.2 «600h + NodeId» становится «600h + Node-ID»;
     - `canopen-od-plan.ru.md` исторический, его не трогать;
   - докомментарии `node_id` и `detail::node_id_out_of_range` (позже его
     заменил `EMB_PANIC`, раздел 8) — в стиле cppreference. `types.hpp` и
     `server.hpp` в `doc-comments-checklist.ru.md` пока не пройдены;
   - в теле коммита — почему Node-ID уходит из шаблона, со ссылкой на
     8aedb30.
2. **etk** `refactor(canopen): give the server its node id at run time`, с
   бампом emblib.
   - изменить: `settings/canopen.hpp`, `comm/can/canopen/server.cpp`.
3. **emblib** `docs(canopen): put the inverter's node id first` — только
   документация, как fd4b9c7.
   - `settings-nvm-design.md`:
     - в журнал решений — запись о Node-ID на 3100h и о сдвиге;
     - в раздел для оператора — таблица «было → стало» из §2.7 и
       предупреждение `params_defaulted` после обновления;
     - пример строк в разделе о мосте (строки 549–550): 3100h → 3120h,
       3107h → 3127h;
     - записи о прошлом (3165h на строках 1345 и 1824) не трогать.
   - `canopen-od-algorithm.ru.md`:
     - §1.5, пример строки: `0x3141` → `0x3161`;
     - текстовый вид строки: `3100h:00 drive.phase_swap` → `3120h:00
       drive.phase_swap`;
     - пример описания: `"index":"0x3141"` → `"index":"0x3161"`;
     - §5.1: 3140h → 3160h; кадры `40 60 31 01 …`,
       `43 60 31 01 0B 00 00 00`, `4F 60 31 07 01 00 00 00`,
       `80 60 31 0A 11 00 09 06`;
     - §5.3: 3141h → 3161h; кадры `23 61 31 01 …`, `60 61 31 01 …`,
       `23 61 31 08 …`, `80 61 31 08 22 00 00 08`, `60 61 31 08 …`;
     - §5.5: ключ 3120h:00, `od.cpp:262:60` и `262 |` вместо 259, ошибка
       `od: 3120h:00 drive.phase_swap and 3120h:00 drive.pwm_frequency
       share the index of a record`.

   «Индекс 16» и «индекс 17» в §5.1 и §5.3 остаются: Node-ID в схеме
   последний.
4. **etk** `feat(settings)!: set the node id, first among the parameters`,
   с бампом emblib до шага 3.
   - изменить: `settings/schema.hpp`, `settings/settings.hpp`,
     `settings/settings.cpp`, `settings/canopen.hpp`,
     `comm/can/canopen/server.hpp`, `comm/can/canopen/server.cpp`,
     `comm/can/canopen/od.cpp`, `main.cpp`;
   - в теле — «For an operator:» с адресами блоков, как в ed02775.
5. **emblib** docs-коммит после замеров: статус этого документа и разделы
   «Размеры» и «Отличия реализации от плана». etk подхватит его со
   следующим бампом.

## 4. Остальные потребители

Каждый переходит при своём следующем бампе emblib.

- **bike** и **psfb** — как шаг 2 etk. `.node_id = 0x01` уходит из
  `settings/canopen.hpp:14`, туда же встаёт константа `node_id`, а
  конструктор обёртки сервера передаёт её в `server_` (член объявлен в
  `comm/can/canopen/server.hpp:62` у bike и `:51` у psfb).
- **h2-hess.**
  - В `src/common/contract/canopen.hpp` пять констант `std::uint8_t
    canopen_node_id = N` (строки 68, 132, 183, 234, 235) становятся
    `co::node_id canopen_node_id{N}`. 26 вызовов `cob_id_of<co::cob_type::X,
    I, id>()` становятся `cob_id_of<co::cob_type::X, I>(id)`. Обе замены
    делает

    ```
    perl -pi -e 's/cob_id_of<(co::cob_type::\w+), (\d+), (\w+)>\(\)/cob_id_of<$1, $2>($3)/g; s/inline constexpr std::uint8_t (canopen(?:_aux)?_node_id) = (\d+);/inline constexpr co::node_id $1\{$2\};/' src/common/contract/canopen.hpp
    ```

    Проба на копии контракта: меняется 31 строка, и все 26 COB-ID
    совпадают со старыми (host g++: старый контракт собран со старой
    emblib, новый — с прототипом).
  - Серверы `chss` (`server.hpp:65`), `pdu` (`:64`), `gpmu` (`:43`) и
    `cshpp` (`:116-121`) теряют `.node_id` в опциях. У `chss` и `pdu`
    остаётся `server<{}, od::context>`. Node-ID из контракта передаётся в
    конструктор, в `server.cpp` каждого приложения.
  - Custom COB-ID из контракта в аргументах шаблона
    (`setup_tpdo<1, pdo_id::custom(chss::rpdo1_cob_id)>` в
    `src/app/cshpp/comm/can/canopen/server.cpp:104`) работают как прежде;
    это проверено пробой.
- **sevpress** сидит на emblib до 8aedb30, где Node-ID и так значение. Его
  переход — отдельная миграция, как в плане словаря. При переходе
  `make(static_cast<std::uint8_t>(cfg.node_id)).value()` становится
  `make(cfg.node_id)` и проверяет значение целиком.
- **Хосты.** Шаги 1 и 2 их не касаются. После шага 4 таблица workbench
  (`od/inverter.od`, строится из `od.cpp`) получит новый объект при
  следующей регенерации. Адрес узла workbench уже берёт из `--node`.

## 5. Проверка

**Тест emblib.** `emb/can/canopen/test/types_test.cpp` содержит только
`static_assert` в безымянном пространстве имён, как остальные тесты; глоб
собирает его в прошивку каждого потребителя.
- `node_id`: `make` для 0, 128 и 261 даёт `nullopt`, для 1 и 127 —
  значение; `node_id{5} == *make(5)`; `sizeof == 1`; тип тривиально
  копируемый, без конструктора по умолчанию и не преобразуется из `int`.
  Что `node_id{v}` не принимает переменную, признак типа не увидит; это
  проверяет негативная проба.
- `cob_id_of` для узлов 1 и 127: 081h, 581h, 67Fh, 77Fh, 181h, 4FFh, 201h,
  57Fh; NMT — 000h, SYNC — 080h. Концептами проверяется, что heartbeat без
  узла, NMT с узлом и PDO без номера не вызываются.
- `nmt_slave` узла 5 выполняет команду себе и всем (адресат 0) и пропускает
  команду узлу 6.

**Негативные пробы, без коммита** (arm-none-eabi-g++, флаги
rev-a-release):

| Проба | Ошибка |
|---|---|
| `node_id{0}`, `node_id{128}`, `node_id{300}` | `call to non-'constexpr' function 'void emb::ensure_failed() [with fixed_string<...auto...> Message = fixed_string<36>{"node_id: the id must be in [1, 127]"}]'` |
| `node_id{v}`, `v` — время выполнения | `call to consteval function 'emb::can::canopen::node_id(v)' is not a constant expression` |
| `node_id x = 5;` | `conversion from 'int' to non-scalar type 'const emb::can::canopen::node_id' requested` |
| `f(v)` для `f(node_id)` и `std::uint8_t v` | `could not convert 'v' from 'uint8_t' {aka 'unsigned char'} to 'emb::can::canopen::node_id'` |
| `node_id{-1}` | `narrowing conversion of '-1' from 'int' to 'unsigned int'` |
| `template<node_id N>` | `'emb::can::canopen::node_id' is not a valid type for a template non-type parameter because it is not structural` |
| `cob_id_of<cob_type::tpdo, 5>(n)` | `static assertion failed: predefined COB-ID exists only for PDO 1..4` |
| старое `cob_id_of<cob_type::tpdo, 1, 1>()` | `no matching function for call to 'cob_id_of<emb::can::canopen::cob_type::tpdo, 1, 1>()'`, в примечании `wrong number of template arguments (3, should be 1)` |
| старое `server<{.node_id = 1}, c>` | `could not convert '{1}' from '<brace-enclosed initializer list>' to 'emb::can::canopen::server_options'` |
| старое `watch_heartbeat<5>(…)` | `invalid use of member function '… watch_heartbeat(emb::can::canopen::node_id, …)'` |

**Прошивка.** Четыре пресета (`rev-a-release`, `rev-a-debug`,
`miniboard-release`, `miniboard-debug`) собираются с `-Werror` после шагов 2
и 4. Прототип шага 4 собрал на всех четырёх все шесть TU etk, которые
доходят до изменённых заголовков: `od.cpp`, `server.cpp`, `main.cpp`,
`settings/apply.cpp`, `section.cpp` и `settings.cpp`.

**Размеры** измерить после реализации так же, как в плане словаря: две
копии дерева с путями одинаковой длины, секции по `arm-none-eabi-size -A`,
`.rodata` без `git.cpp.obj`. Пробы шага 2 дают ориентир.
`sizeof(comm::can::canopen::server)` растёт с 1744 до 1760 B, а этот объект
живёт на стеке `main`. В объектах rev-a-release `.text` у `server.cpp.obj`
растёт на 8 B, у `main.cpp.obj` — на 72 B; `od.cpp.obj` не меняется.

**Стенд** (ucan-monitor или демон workbench, старая и новая прошивка):
1. Узел 1 после шага 2 ведёт себя как на старой прошивке:
   - heartbeat — 701h;
   - после NMT start (000h: 01 01) идут TPDO 181h–481h со своими периодами;
   - RPDO 201h и 301h от VCU принимаются;
   - SDO 601h → 581h;
   - EMCY 081h приходит, если в operational прекратить RPDO дольше 350 ms.
2. NMT: команды узлу 1 и всем (0) выполняются, узлу 2 — нет.
3. Прошивку шага 4 записать на устройство с записью прежней прошивки.
   3000h:0D (загружено) — 67, 3000h:0F (отвергнуто) — 0, 3000h:10 (без
   значения) — 1, горит `params_defaulted`. Параметры читаются на новых
   местах с прежними значениями: например, `drive.phase_swap` — 3120h:01,
   `temperature_sensor.mcu_time_constant` — 3242h:01.
4. 3100h:01 читается как 1, 3100h:07 — 1 (`on_restart`). Запись 5 в
   3100h:01 проходит; 3100h:02 остаётся 1, 3000h:11 становится true, на
   шине до перезапуска ничего не меняется. 3100h:08 отвечает 06010000h.
   Запись 0 и 128 в 3100h:01 даёт 06090030h.
5. 2101h:02 с подписью `save` (65766173h) перезапускает узел. После этого:
   - 3100h:01, 3100h:02 и 3100h:03 читаются как 5;
   - heartbeat — 705h;
   - SDO 605h → 585h;
   - TPDO 185h–485h;
   - RPDO ожидаются на 205h и 305h; пока VCU шлёт на 201h, поднимается
     авария связи с VCU, как и должно быть;
   - NMT для узла 5 выполняется, для узла 1 — нет.
6. 2102h:01 с подписью `load` (64616F6Ch), затем 2101h:02 — узел снова 1.

## 6. Ловушки

- **`throw` в consteval** под `-fno-exceptions` не компилируется (§8 плана
  словаря). Конструктор отказывает через `EMB_PANIC`, как `param` в
  настройках.
- **Признак типа не видит `consteval`.** `std::is_constructible_v<node_id,
  std::uint8_t>` — true: признак смотрит на объявления, а не на то,
  вычислимо ли выражение при компиляции. Что `node_id{v}` с переменной не
  компилируется, проверяет только негативная проба.
- **`explicit` обязателен.** Без него `f(5)` молча превращал бы число в
  `node_id`, а конструктор сервера принимал бы голое число.
- **`node_id` не структурный тип.** Его член закрыт, поэтому `node_id` не
  может быть параметром шаблона, а `server_options` с таким членом
  перестал бы им быть. По той же причине `watch<Remote>` переходит на
  значение.
- **Сужение до проверки.** `make` берёт `unsigned` и проверяет его до
  приведения к `uint8_t`. Иначе 261 прошёл бы как 5, как в sevpress.
- **`std::uint32_t` на ARM — это `unsigned long`.** Значение настройки
  передаётся в `make(unsigned)` без предупреждений `-Wconversion`: ширина
  та же, это проверила сборка `settings.cpp`. Границы параметра
  записываются как `std::uint32_t{node_id::min}`.
- **Порядок членов `sdo_server`.** COB-ID объявляются за `bus_` и до
  `dictionary_`, иначе список инициализации нарушит `-Wreorder`.
- **Параметр — запись.** Его строка в `od.cpp` стоит на 00h, но 00h —
  наибольший подындекс. Node-ID пишут в 3100h:01, применённый читают на
  3100h:02.
- **Порядок групп `od_check` не проверяет.** Его держит только правило:
  группы перечислены в порядке, в котором их впервые называют строки.

## 7. Размеры

Измерено 23 октября 2026 года. Все четыре пресета собраны трижды в одном
дереве: на fa7d3cf, после шага 2 и после шага 4. Секции — по
`arm-none-eabi-size -A`, FLASH — сумма `.apm32_isr_vector`, `.text`,
`.rodata`, `.ARM`, `.init_array`, `.fini_array` и `.data`. `git.cpp.obj`
ничего не кладёт в `.rodata`: из сведений о коммите образ берёт только
дату, describe и ветку, слитые с другими строками, так что `.rodata`
сравнивается без поправки.

| Пресет | Шаги 1–2, `.text` | Шаг 4, `.text` | Шаг 4, `.rodata` | Шаг 4, `.data` | Δ FLASH всего |
|---|---:|---:|---:|---:|---:|
| rev-a-release | +80 | +32 | +64 | +12 | +188 |
| rev-a-debug | +196 | +768 | +296 | +12 | +1 240 |
| miniboard-release | +56 | +28 | +64 | +12 | +160 |
| miniboard-debug | +800 | +764 | +304 | +12 | +1 840 |

После шагов 1–2 `.rodata` в debug меньше на 32 и 40 байт, в release не
меняется; `.data` и `.bss` не меняются. Рост `.text` в rev-a-release, 80
байт, — сумма проб из раздела 5: 8 байт `server.cpp.obj` и 72 байта
`main.cpp.obj`. COB-ID теперь читаются из частей сервера, а не вшиты в
команды. Объект сервера на стеке `main` вырос с 1 744 до 1 760 байт.

Шаг 4 добавил 12 байт `.data`, ячейку параметра в трёх образах настроек.
`.bss` не изменился. `od::table` в rev-a-release — 2 452 байта: 2 448 на
записи, составные объекты и массивы и 4 на отпечаток. Описание выросло с
223 строк и 42 489 байт до 225 строк и 42 725 байт и одинаково во всех
четырёх пресетах. Ни одна из 537 строк описания, путей и наименований, в
образ не попала; меток в `nm` нет; `.hex` раскодируется ровно в `.bin`.
clang считает `od.cpp` за 1 944 576 шагов constexpr против 1 928 192 до
шага 4, с точностью до 1 000.

## 8. Отличия реализации от плана

- Размеры мерились не в двух копиях дерева, а в одном, сборка за сборкой:
  пути при этом совпадают сами собой, а поправка на `git.cpp.obj` не
  нужна (раздел 7).
- clang-format: пустые тела конструкторов `hb_producer` и `emcy_producer`
  стоят на двух строках, конструктор `sdo_server` уместился в одну.
  `nmt_slave.hpp` и `server.hpp` сохранили прежние ручные отступления от
  него, например `case` в одну строку.
- В `settings.hpp` etk `<emb/can/canopen/types.hpp>` подключается после
  заголовков приложения, как в `settings.cpp`, а не перед ними, как в
  прототипе.
- Прежние «около 1 933 000» шагов constexpr мерились с шагом по умолчанию,
  10 000. С точностью до 1 000 база — 1 928 192, так что Node-ID и группа
  `canopen` стоят около 16 000 шагов.
- Шаг 3 обновил в описании алгоритма не только примеры, но и счётчики etk:
  68 составных объектов, 24 группы, 157 строк, 2 448 байт записей, размер
  описания и число шагов constexpr.
- Шаги 0 и 5 добавились к плану по ходу работы.
- После плана отказ при компиляции идёт через `EMB_PANIC` из
  `emb/panic.hpp`, у `node_id` и во всей emblib, а не через вызов
  объявленной, но не определённой функции: причину диагностика показывает
  текстом в строке ошибки, и своя функция на каждую причину не нужна.
  Пробы `node_id{0}`, `node_id{128}` и `node_id{300}` раздела 5
  перепроверены с новым текстом ошибки.
