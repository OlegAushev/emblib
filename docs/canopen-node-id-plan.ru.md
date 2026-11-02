# Node-ID CANopen-сервера: значение вместо параметра шаблона — план

Статус: сделано 23 октября 2026 года (emblib c8dc7e4, e0ac50e, ee819d3).
Отличия реализации от плана — в разделе 6.

Конструкции раздела 2 проверены прототипом в копии дерева на
arm-none-eabi-g++ 15.2 с флагами Cortex-M4F, включая `-Werror`.

Все TU emblib под `emb/can`, включая новый тест, собраны в release- и
debug-сборке. Тест и проба с общим заголовком нескольких узлов собраны
также host g++ 16.2 и clang 22 через `clang-check`. Двенадцать негативных
проб дают ошибки из раздела 4.

Что меняется. Node-ID перестаёт быть параметром шаблона `server` и шести
классов `detail`. Он приходит в конструктор сервера значением типа
`node_id`, а COB-ID служб считаются из него при создании. Число PDO и
ёмкость очереди остаются в `server_options`, потому что задают размеры
массивов. API emblib ломается. При том же Node-ID на шине ничего не
меняется.

## Решения

- 2026-10-23. Node-ID — значение типа `node_id`, аргумент конструктора
  `server`. В `server_options` остаются `tpdo_count`, `rpdo_count` и
  `rx_queue_capacity`: они задают размеры массивов. Шаблон давал две вещи.
  Первая — проверка диапазона при компиляции; её сохраняет конструктор
  `consteval`, `node_id{1}`. Вторая — COB-ID как непосредственные
  операнды, то есть пара тактов на кадр и десяток байт. Отвергнутый
  вариант — оставить шаблон
  и задавать Node-ID пресетом сборки. Тогда на каждый адрес нужен свой
  образ, а хосты уже берут адрес снаружи.
- 2026-10-23. Константа становится `node_id` через явный конструктор
  `consteval`, `node_id{1}`, а значение времени выполнения — через `make`,
  который возвращает `std::optional`. Так же разделены `std::format_string`,
  чей конструктор `consteval` проверяет строку формата, и
  `std::runtime_format`. Диапазон проверяет только `make`, конструктор
  зовёт его, так что отдельного предиката нет. Открыты `min` и `max`: по ним
  задаются границы настройки. Конструктор отказывает так же, как
  `declare_parameter` в настройках: через `EMB_PANIC` (`emb/panic.hpp`), чью
  причину диагностика показывает в строке ошибки; `throw` в consteval под
  `-fno-exceptions` не компилируется (§6 плана словаря). Отвергнутые
  варианты:
  - фабрика `literal(v)`: имя говорит, как выглядит аргумент, а нужна
    константа — `literal(base + 2)` тоже работает;
  - `literal<V>()` со `static_assert`;
  - открытый предикат `is_valid`: на тот же вопрос отвечает `make(v)`.
- 2026-10-23. В `cob_id_of` служба и номер PDO остаются аргументами
  шаблона, а Node-ID становится аргументом функции. Функции constexpr,
  поэтому COB-ID в заголовке, общем для нескольких узлов, по-прежнему
  вычисляются при компиляции. Отвергнутый вариант — всё значениями. Тогда
  номер PDO и класс службы проверялись бы только во время выполнения, а на
  устройстве они всегда константы.
- 2026-10-23. Node-ID читается один раз, при запуске. Новый Node-ID
  вступает в силу после перезапуска (`apply_policy::on_restart`).
  Отвергнутый вариант — смена по NMT reset communication, как в CiA 301.
  У `transport` нет снятия фильтров (`detail/rpdo_consumer.hpp:46-47`).
- 2026-10-23. `watch_heartbeat` и `hb_consumer::watch` тоже берут
  удалённый Node-ID значением, а `on_lost` получает `node_id`. Их не
  вызывает ни один потребитель. Параметром шаблона `node_id` быть не может:
  это не структурный тип (раздел 5).
- 2026-10-23. Аксессора Node-ID у сервера нет. У сервера до 8aedb30 был
  `id()`, но сейчас он никому не нужен.

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
- Параметр настроек в словаре — запись (`od_settings.hpp:242-255`). Её
  строка в таблице стоит на 00h, но 00h — наибольший подындекс, 09h.
  Запрошенное значение — 01h, применённое — 02h, сохранённое — 03h,
  политика — 07h; 08h применяет параметр, 09h отбрасывает запрос.

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
перегрузки не находится; тест проверяет это концептами (раздел 4). Функции
constexpr, и COB-ID из общего заголовка узлов годятся в аргументы шаблона,
как в `setup_tpdo<1, pdo_id::custom(peer::rpdo1_cob_id)>`.

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
так же.

## 3. План работ

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
     заменил `EMB_PANIC`, раздел 6) — в стиле cppreference. `types.hpp` и
     `server.hpp` в `doc-comments-checklist.ru.md` пока не пройдены;
   - в теле коммита — почему Node-ID уходит из шаблона, со ссылкой на
     8aedb30.
2. **emblib** docs-коммит после реализации: статус этого документа и
   раздел «Отличия реализации от плана».

## 4. Проверка

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
release-сборки):

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

## 5. Ловушки

- **`throw` в consteval** под `-fno-exceptions` не компилируется (§6 плана
  словаря). Конструктор отказывает через `EMB_PANIC`, как
  `declare_parameter` в настройках.
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
  приведения к `uint8_t`. Иначе 261 прошёл бы как 5.
- **`std::uint32_t` на ARM — это `unsigned long`.** Значение настройки типа
  `std::uint32_t` передаётся в `make(unsigned)` без предупреждений
  `-Wconversion`: ширина та же, это проверила сборка прототипа. Её границы
  записываются как `std::uint32_t{node_id::min}`.
- **Порядок членов `sdo_server`.** COB-ID объявляются за `bus_` и до
  `dictionary_`, иначе список инициализации нарушит `-Wreorder`.
- **Параметр — запись.** Его строка в таблице стоит на 00h, но 00h —
  наибольший подындекс. Node-ID пишут в подындекс 01h, применённый читают
  в 02h.
- **Порядок групп `od_check` не проверяет.** Его держит только правило:
  группы перечислены в порядке, в котором их впервые называют строки.

## 6. Отличия реализации от плана

- clang-format: пустые тела конструкторов `hb_producer` и `emcy_producer`
  стоят на двух строках, конструктор `sdo_server` уместился в одну.
  `nmt_slave.hpp` и `server.hpp` сохранили прежние ручные отступления от
  него, например `case` в одну строку.
- Шаги 0 и 2 добавились к плану по ходу работы.
- После плана отказ при компиляции идёт через `EMB_PANIC` из
  `emb/panic.hpp`, у `node_id` и во всей emblib, а не через вызов
  объявленной, но не определённой функции: причину диагностика показывает
  текстом в строке ошибки, и своя функция на каждую причину не нужна.
  Пробы `node_id{0}`, `node_id{128}` и `node_id{300}` раздела 4
  перепроверены с новым текстом ошибки.
