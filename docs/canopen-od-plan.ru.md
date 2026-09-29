# Словарь объектов CANopen: constexpr-таблица и контекст обработчиков — план

Статус: emblib и etk сделаны 29 сентября 2026 года, все четыре пресета
собираются; на стенде не проверялось, остальные потребители не переведены
(раздел 5). Отличия реализации от плана — в разделе 9, измеренные размеры —
в разделе 7.

План составлен 28–29 сентября 2026 года с нуля. Предыдущая версия этого
документа (emblib 927f8d9) при этом не использовалась; этот текст её
заменяет. Каждая C++-конструкция ниже проверена пробами на
arm-none-eabi-g++ 15.2 с флагами `od.cpp` etk, включая `-Werror`, и на host
g++ 16.2; большинство — также на clang 22 через `clang-check`.

Что меняется. Таблица словаря становится constexpr-объектом: строки
сортируются и проверяются при компиляции, образ хранит 16-байтовые записи в
`.rodata`, а имена и единицы остаются только в исходнике. Обработчики
типизированы: тип объекта выводится из сигнатуры обработчика, контекст
приложения приходит по ссылке. Строки параметров настроек строятся из
схемы. API emblib ломается; на шине всё по-прежнему, кроме одного случая со
строковыми объектами (§2.7).

## Решения

- 2026-09-28. В первой серии переводится только etk. Остальные потребители
  переходят при своём следующем бампе emblib (раздел 5).
- 2026-09-28. Обработчики типизированные (§2.4). Отвергнутый вариант —
  сырые обработчики `(Ctx&, arg, od_value)` с явной колонкой типа: в нём
  совпадение типа строки и обработчика проверял бы только debug-`assert`.
- 2026-09-28. 0x1011:04 обслуживает сервер, как сейчас (§2.6). Отвергнутый
  вариант — обычная строка приложения: её обработчику нужен поиск по
  словарю, в котором она сама лежит, а chss, pdu и gpmu поменяли бы
  поведение на шине.
- 2026-09-28. У строковых объектов один курсор на сервер (§2.6).
- 2026-09-29. Построитель `od_rw<G, S>` входит в набор, хотя в etk ему пока
  нечего обслуживать.
- Контекст etk минимальный: привод, датчик Холла и `dc_test` (§2.8).
  Сервисы уровня модуля — telemetry, settings, trouble, sysinfo,
  планировщик, dbg probe — остаются вне его.

## 1. Что сейчас

- `emb/can/canopen/od.hpp`: строка — это `od_entry {od_key, od_object}`. В
  `od_object` четыре `char const*`, access, тип, `std::optional<od_value>`
  умолчания и два указателя, `read()` и `write(od_value)`, которым некуда
  передать контекст. На ARM строка занимает 44 B.
- Таблица etk (`src/app/inverter/comm/can/canopen/od.cpp`, 165 строк) —
  неконстантный `std::array`, потому что `sdo_server` сортирует её
  `std::sort` при старте. Поэтому она лежит в `.data`: 7 260 B RAM и
  столько же flash под образ инициализации. Дубликаты ключей ловит
  `assert`, то есть только в debug.
- Обработчики берут состояние из синглтонов (`md::motor_drive::instance()`
  9 раз, `hall::angle_sensor::instance()`/`exists()`), из extern-глобалов
  `od::dc_test::a/b/c`, которые читает `server.cpp`, и из static-счётчиков
  пяти строковых объектов. Каждому из 58 параметров настроек нужна своя
  пара thunk'ов `read_param<Name>`/`write_param<Name>`.
- 0x1011:04 (восстановить умолчание одного параметра): сервер перехватывает
  запись до поиска и пишет в строку-цель её `default_value`. Строка
  0x1011:04 в таблице etk есть, но её writer никто не вызывает.
- Тот же API у bike, psfb и четырёх приложений h2-hess; sevpress сидит на
  emblib на 203 коммита старше (раздел 5). Исходник таблицы никто не
  разбирает — ни скрипты, ни codegen; ucan-monitor держит собственную
  рукописную копию метаданных.

## 2. Дизайн

### 2.1 Типы (`emb/can/canopen/od.hpp`, переписывается)

Остаются `od_value`, `od_scalar`, `od_value_type`, `od_access`,
`od_read_result`, `od_write_result`, `make_od_value`, `to_raw` и
`od_data_type_sizes`. Добавляются `static_assert`, связывающие коды 0–7
`od_value_type` с индексами альтернатив `od_value`, и
`constexpr od_alternative_of(od_value_type)`: для `exec` и `string` он
возвращает индекс `uint32`.

```cpp
struct od_key {
  std::uint16_t index;
  std::uint8_t subindex;
  friend constexpr bool operator==(od_key, od_key) = default;
  friend constexpr auto operator<=>(od_key, od_key) = default;
};

constexpr bool od_readable(od_access a);  // a != wo
constexpr bool od_writable(od_access a);  // a == rw || a == wo

// 1011h:04. Запись несёт ключ объекта, чьё умолчание восстановить:
// байты 0-1 — индекс (LE), байт 2 — подындекс, байт 3 не читается.
inline constexpr od_key od_restore_default_key{0x1011, 0x04};

template<typename Ctx>
using od_read_fn = od_read_result (*)(Ctx&, std::uint16_t arg);
template<typename Ctx>
using od_write_fn = od_write_result (*)(Ctx&, std::uint16_t arg, od_value);
template<typename Ctx>
using od_restore_fn = od_write_result (*)(Ctx&, std::uint16_t arg);

// Пространство индексов вне словаря, которое строки обслуживают через
// arg: например, параметры схемы настроек.
struct od_catalog {
  std::string_view what;  // "settings parameter"
  std::span<std::string_view const> names;
  std::span<bool const> exposed;
};

// Что обслуживает объект. Строят построители (§2.4) и мост (§2.5).
template<typename Ctx>
struct od_binding {
  od_access access;
  od_value_type type;
  od_read_fn<Ctx> read = nullptr;       // есть ⇔ od_readable(access)
  od_write_fn<Ctx> write = nullptr;     // есть ⇔ od_writable(access)
  std::uint16_t arg = 0;
  od_restore_fn<Ctx> restore = nullptr; // объект восстанавливается 1011h:04
  od_catalog const* catalog = nullptr;
  bool diagnosed = false;               // построитель уже выдал ошибку
};

// Строка, как её пишет приложение. Существует только при компиляции.
template<typename Ctx>
struct od_row {
  od_key key;
  std::string_view category, subcategory, name, unit;
  od_binding<Ctx> binding;
};

// Что от строки остаётся в образе: 16 B на Cortex-M4.
template<typename Ctx>
struct od_entry {
  std::uint16_t index;
  std::uint8_t subindex;
  od_value_type type;
  od_access access;
  bool restorable;
  std::uint16_t arg;
  od_read_fn<Ctx> read;
  od_write_fn<Ctx> write;
  constexpr od_key key() const { return {index, subindex}; }
};
```

Уходят `od_object`, прежний `od_entry`, пять рукописных операторов
сравнения, `od_no_read`/`od_no_write`, `od_read_var`/`od_write_var` (их
никто не использует), `has_*_permission` и `default_value`.

`arg` передаёт строкам моста настроек индекс параметра и помещается в байты
выравнивания записи. Типизированным обработчикам он не нужен: семейства
вроде `Tpwr<I>` остаются шаблонами, как сейчас.

### 2.2 Словарь (`emb/can/canopen/od_dictionary.hpp`, новый)

```cpp
template<typename Ctx, std::size_t N>
class od_dictionary {                      // создаёт только make_dictionary
public:
  constexpr std::span<od_entry<Ctx> const> entries() const;
  constexpr od_restore_fn<Ctx> restore() const;

private:
  consteval od_dictionary() = default;
  template<auto const& Rows>
  friend consteval auto make_dictionary();

  std::array<od_entry<Ctx>, N> entries_{}; // по возрастанию ключа
  od_restore_fn<Ctx> restore_ = nullptr;   // единственная (T12)
};

template<auto const& Rows>                 // C-массив od_row<Ctx>
consteval auto make_dictionary();

template<typename Ctx>
class od_view {                            // 12 B: span и restore
public:
  template<std::size_t N>
  constexpr od_view(od_dictionary<Ctx, N> const&);
  template<std::size_t N>
  od_view(od_dictionary<Ctx, N> const&&) = delete;

  constexpr od_entry<Ctx> const* find(od_key) const; // ranges::lower_bound
  constexpr od_restore_fn<Ctx> restore() const;
  constexpr std::span<od_entry<Ctx> const> entries() const;
};
```

`make_dictionary` делает `static_assert(detail::od_check(Rows).empty(),
detail::od_check(Rows))`, копирует привязки в записи (`restorable` — это
`restore != nullptr`), поднимает restore-функцию в словарь и сортирует
записи `std::ranges::sort` по `key()`. Отсортированность и проверенность
становятся свойством типа: другим путём `od_dictionary` не построить, а
сервер принимает только `od_view`, который строится из словаря. Вид на
временный словарь запрещён удалённым конструктором.

Строки передаются аргументом шаблона, потому что по параметру
consteval-функции `static_assert` не написать. Массив строк должен быть
`inline constexpr` в именованном пространстве имён: в безымянном, как и без
`inline`, GCC при `-O0` кладёт таблицу и имена в объектный файл.

### 2.3 Проверки (`emb/can/canopen/detail/od_check.hpp`, новый)

`od_check(rows)` возвращает `std::string`: пустую, если таблица верна, иначе
текст первого нарушения. Текст собирается только при ошибке и уходит в
`static_assert` как сообщение (P2741; GCC 15, GCC 16 и clang 22 это
принимают). Строка таблицы в нём выглядит как
`3002h:01 config/drive/phase_swap`. Диагностика GCC занимает несколько
строк, среди них «required from here» на строке `make_dictionary<rows>()`
в приложении.

Сначала идут проверки отдельных строк в порядке исходника, затем проверки
пар, затем каталогов. Если у какой-то строки стоит `diagnosed`, `od_check`
сразу возвращает пустую строку: ошибка построителя (§2.4, §2.5) остаётся
единственной. Без этого за ней следует вторая, сбивающая с толку: для
неизвестного имени параметра, например, виноватым оказывается параметр 0.

| # | Правило | Сообщение |
|---|---|---|
| T1 | index ≥ 1000h | `od: <row>: indices below 1000h are reserved` |
| T2 | категория, подкатегория и имя не пусты | `od: <row>: category, subcategory and name must not be empty` |
| T3 | readable(access) ⇔ есть reader | `od: <row>: readable but has no reader` / `od: <row>: write-only but has a reader` |
| T4 | writable(access) ⇔ есть writer; 1011h:04 — исключение | `od: <row>: writable but has no writer` / `od: <row>: read-only but has a writer` |
| T5 | exec ⇒ wo | `od: <row>: an exec object must be wo` |
| T6 | string не записывается | `od: <row>: a string object must not be writable` |
| T7 | restore ⇒ строка записываемая | `od: <row>: restorable but not writable` |
| T8 | строка на 1011h:04 — ровно `od_restore_default` | `od: <row>: the server serves this key; bind it with od_restore_default` |
| T9 | у строки с каталогом arg < числа элементов | `od: <row>: arg is outside its catalog` |
| T10 | ключи уникальны | `od: <row> and <row> have the same key` |
| T11 | тройки имён уникальны | `od: <row> and <row> have the same name` |
| T12 | в словаре одна restore-функция | `od: <row> and <row> restore through different functions` |
| T13 | две строки без каталога не делят пару (ненулевой reader, arg) или (ненулевой writer, arg) | `od: <row> and <row> are read by the same handler` / `… written by the same handler` |
| T14 | у каждого exposed-элемента каталога ровно одна строка, у скрытого — ни одной | `od: settings parameter 'x' has no row` / `… has two rows: <row> and <row>` / `… is not exposed but has a row: <row>` |

T13 ловит скопированную строку вроде второго `Tmot<0>`. Пустые обработчики
в ней не сравниваются, иначе совпали бы все ro-строки по пустому writer'у.

Ошибки построителей выдаются на строке таблицы:

| Построитель | Сообщение |
|---|---|
| `od_ro`, `od_const` | `od: a reader takes nothing or a reference to the context, and returns an od_value scalar, a type wrapping one, or std::expected of either` |
| `od_wo` | `od: a writer takes the value, optionally after a reference to the context, and returns void or od_write_result` |
| `od_rw` | два сообщения выше, плюс `od: the reader and the writer of an rw object disagree on the type` |
| `od_exec` | `od: a command takes nothing or a reference to the context, and returns void or od_write_result` |
| `od_text` | `od: a text reader takes nothing or a reference to the context, and returns std::string_view or char const*` |
| `od_settings` | S1: существующее `Unknown parameter 'x'` из `schema.hpp`; S2: `od: settings parameter 'x' is not exposed`; S3: `od: settings parameter 'x' is not writable; bind it with ro` |

Проверять не нужно, это следует из построения: тип строки настроек равен
типу параметра в схеме, тип типизированной строки — типу обработчика.

Остаётся рантайму и ревью:
- альтернатива, которую возвращает привязка, собранная вручную, — в
  сервере её проверяет debug-`assert`;
- `Tpwr<I>` за пределом установленных датчиков (`object_not_found`, как
  сейчас);
- единицы измерения и копия метаданных в ucan-monitor;
- перепутанные местами обработчики одного типа, например `Tpwr<1>` и
  `Tpwr<2>`: против этого — конвертация скриптом (раздел 4).

Отвергнуто:
- `throw` в consteval (так сообщает об ошибках host-словарь cannet): под
  `-fno-exceptions` это ошибка компиляции даже в недостижимой ветке;
- вызов неопределённой функции (приём из `schema.hpp`): диагностика
  называет функцию, но не строку. А если передать таблицу временным
  объектом, GCC печатает её целиком: 499 KB в одной строке на 171 строку
  таблицы.

### 2.4 Типизированные обработчики (`emb/can/canopen/od_handlers.hpp`, новый)

Обработчик — функция или лямбда без захвата (`+[]`). Первым параметром он
может принимать `Ctx&` или `Ctx const&`, а может обходиться без контекста.
`noexcept` допускается.

| Построитель | Сигнатура обработчика | access, тип объекта |
|---|---|---|
| `od_ro<F>`, `od_const<F>` | `T f([ctx])` или `std::expected<T, sdo_abort_code> f([ctx])` | ro или const_, скаляр T |
| `od_wo<S>` | `void s([ctx&,] T)` или `od_write_result s([ctx&,] T)` | wo, скаляр T |
| `od_rw<G, S>` | G — как у `od_ro`, S — как у `od_wo`; скаляры G и S совпадают, обёртки могут различаться (`hz_f32` и `float`) | rw, общий скаляр |
| `od_exec<F>` | `void f([ctx])` или `od_write_result f([ctx])` | wo, exec; значение не передаётся |
| `od_text<F>` | `std::string_view f([ctx])` или `char const* f([ctx])` | const_, string |
| `od_restore_default` | — | wo, exec, без обработчиков |

T — скаляр `od_value` или обёртка над ним (`wraps_od_scalar`:
`units::named_unit`, `emb::clamped`); тип объекта — скаляр T. Построители —
это переменные-шаблоны с `consteval operator od_binding<Ctx>()`, так что
`Ctx` выводится из `od_row<context>`. Thunk'и `read_thunk`, `write_thunk`,
`command_thunk` и `text_thunk` — `constexpr`, чтобы тесты могли звать их
через записи. `text_thunk` отдаёт байты `[4·word, 4·word + 4)` строки,
нулями за её концом.

`od_rw` нужен для rw-объектов вне настроек. restore у него нет:
восстановление умолчаний остаётся за мостом настроек. В etk таких строк
пока нет (все 58 rw-строк — настройки), так что `od_rw` проверяют только
тесты emblib.

Во что это обходится. При `-O3` и `-Os` thunk инструкция в инструкцию
совпадает с рукописной функцией того же назначения: так на ARM проверены
чтение скаляра, обёртки и `expected`, запись с `void` и с
`od_write_result`. При `-O0` на строку добавляется один вызов.

### 2.5 Мост к настройкам (`emb/can/canopen/od_settings.hpp`, новый)

Зависимость односторонняя: canopen знает о settings, settings о canopen —
нет.

```cpp
template<auto& Schema, auto GetAt, auto SetAt, auto RestoreAt>
  requires /* сигнатуры by-index аксессоров: get_at, set_at,
              restore_default_at */
struct od_settings {
  template<fixed_string Name>
  static constexpr /* binding */ rw{};  // read + write + restore, arg = index
  template<fixed_string Name>
  static constexpr /* binding */ ro{};
  static constexpr od_catalog catalog{"settings parameter", names, exposed};
};
```

- Тип объекта берётся из схемы (`od_type_of(settings::value_type)`).
- Ошибки аксессоров маппятся так же, как в нынешнем `write_parameter`:
  `unknown_parameter` → `object_not_found`, `read_only` →
  `write_to_read_only`, `type_mismatch` → `data_type_mismatch`,
  `out_of_range` → `value_range_exceeded`.
- Значение, которого нет среди альтернатив `settings::value` (узкие целые),
  даёт `data_type_mismatch`.
- `rw` восстанавливается через 1011h:04 функцией `RestoreAt`, то есть
  `settings::restore_default_at`. Умолчания в образ не попадают.
- Покрытие схемы проверяет T14 через `catalog`, который несёт каждая строка
  моста. Отсюда граница: словарь без единой строки настроек не проверяется —
  каталог до `make_dictionary` просто не доходит.
- При `-O3` от моста остаются три общие функции вместо 116 thunk'ов.

### 2.6 Сервер (`detail/sdo_server.hpp`, `server.hpp`)

`detail::sdo_server<NodeId, Ctx>(transport&, od_view<Ctx>, Ctx&)` и
`server<Opt, Ctx>(clock, transport&, od_view<Ctx>, Ctx&)`; перегрузки с
`Ctx&&` удалены. Сервер держит контекст по ссылке: состояние приложения
принадлежит приложению. `init_dictionary`, `std::sort` и собственный `find`
сервера уходят.

```
read:    !od_readable(e.access)         -> read_from_write_only
         arg = e.type == string ? text_word(e.key()) : e.arg
         v = e.read(ctx_, arg);  !v     -> v.error()
         assert(v->index() == od_alternative_of(e.type))
         data = to_raw(*v)
         string: word = (в data есть 0) ? 0 : word + 1
         n = (4 - od_data_type_sizes[e.type]) & 3

write:   !od_writable(e.access)         -> write_to_read_only
         e.write(ctx_, e.arg, make_od_value(data, e.type))

1011:04 (перехват записи до поиска, как сейчас):
         target = {data[0] | data[1] << 8, data[2]}
         нет цели                       -> object_not_found
         !od_writable(цель.access)      -> write_to_read_only
         !цель.restorable               -> no_data_available
         иначе dictionary_.restore()(ctx_, цель.arg)
```

Ошибку чтения сервер возвращает до всякого `->`: debug-сборки определяют
`DEBUG`, а не `NDEBUG`, так что `assert` работает, а `->` на ошибке —
неопределённое поведение.

Курсор строковых объектов один на сервер: `{od_key key; std::uint16_t
word}`. `text_word(key)` сбрасывает его в `{key, 0}`, если он стоит на
другом ключе; после слова с нулевым байтом он тоже возвращается к 0.

### 2.7 Шина

Относительно нынешней emblib (etk, bike, psfb, h2-hess) на шине ничего не
меняется: коды 06010001, 06010002, 06020000, 05040001 и 08000024, порядок
проверок 1011h:04, отсутствие ответа на abort клиента, молчаливый сброс
кадров при полной очереди, `n` по объявленному типу, нулевые данные в
ответе на запись. Для строк 'abcd', '' и 'abcdefg' байты совпадают с
нынешними.

Меняется одно. Если клиент прервёт чтение строкового объекта на середине и
начнёт читать другой, первый потом читается с начала. Сейчас его счётчик
застревает на середине, и следующее чтение даёт мусор. `StringReader` в
ucan-monitor читает каждую строку до нуля и строки не перемежает
(`server_od_utils.cpp:41-62`).

### 2.8 etk: контекст и таблица

`src/app/inverter/comm/can/canopen/od.hpp` объявляет `md::motor_drive` и
`hall::angle_sensor` вперёд — md по-прежнему не видит comm:

```cpp
struct duty_cycles {
  float a = 0.f;
  float b = 0.f;
  float c = 0.f;
};

struct context {
  md::motor_drive& drive;
  hall::angle_sensor* hall_sensor = nullptr; // вместо exists()/instance()
  duty_cycles dc_test{};                     // вместо глобалов od::dc_test
};

extern emb::can::canopen::od_view<context> const dictionary;
```

`od.cpp`: обработчики лежат в безымянном пространстве имён, таблица — в
`od`:

```cpp
using param = emb::can::canopen::od_settings<settings::schema,
                                             settings::get_at,
                                             settings::set_at,
                                             settings::restore_default_at>;

// clang-format off
inline constexpr emb::can::canopen::od_row<context> rows[] = {
{{0x1008, 0x00}, "info",   "sys",    "device_name",               "",   od_text<device_name>},
{{0x1010, 0x01}, "ctl",    "sys",    "save_all_parameters",       "",   od_exec<save_all_parameters>},
{{0x1011, 0x04}, "ctl",    "sys",    "restore_default_parameter", "",   od_restore_default},
{{0x1018, 0x04}, "info",   "sys",    "serial_number",             "",   od_const<apm32::f4::core::serial_number>},
{{0x2001, 0x03}, "ctl",    "drive",  "set_angle_correction",      "°",  od_wo<set_angle_correction>},
{{0x5000, 0x11}, "watch",  "elec",   "Vdc",                       "V",  od_ro<Vdc>},
{{0x5000, 0x24}, "watch",  "temp",   "Tpwr_a",                    "°C", od_ro<Tpwr<0>>},
{{0x5000, 0xF1}, "watch",  "logger", "ch0",                       "",   od_ro<logger<probe_channel::ch0>>},
{{0x3002, 0x01}, "config", "drive",  "phase_swap",                "",   param::rw<"drive.phase_swap">},
// ...
};
// clang-format on

constexpr auto table = emb::can::canopen::make_dictionary<rows>();
constinit emb::can::canopen::od_view<context> const dictionary{table};
```

Здесь `Vdc` — это `emb::units::volt_f32 Vdc()`, `Tpwr<I>` возвращает
`std::expected<float, sdo_abort_code>`, `set_angle_correction` — это
`void (context&, emb::units::edeg_f32)`.

Владение и конкурентность:
- Контекст хранит обёртка `comm::can::canopen::server` в поле
  `od::context ctx_`, объявленном раньше `server_`; emblib держит ссылку на
  него. Обёртка не копируется и не перемещается, так что ссылка не
  висит.
- Конструктор обёртки: `server(emb::can::transport&,
  od_view<od::context>, od::context, md::ctl::vcu_inbox&)`. Словарь
  по-прежнему передаёт `main`.
- `make_run_command()` читает `ctx_.dc_test` и `ctx_.drive.motor().p`,
  `handle_rpdo1` вызывает `ctx_.drive.reset_connection_timeout()`: оба
  `instance()` уходят и из `server.cpp`.
- `main.cpp` передаёт `{.drive = drive, .hall_sensor =
  std::get_if<hall::angle_sensor>(&angle_sensor)}`. Альтернатива Холла
  emplace'ится раньше, чем строится сервер (main.cpp:131 и 186), и больше
  не меняется — на это уже полагается `interrupt::enable_hall`.
- Все обработчики и `make_run_command` работают внутри `can_server.run()` в
  главном цикле; ISR контекст не трогают.
- `context const&` не делает `drive` константным: это член-ссылка.
  Выбирать `const&` или `&` по тому, что обработчик меняет, — соглашение,
  а не гарантия.

## 3. План работ

Коммиты идут в обычном порядке: сначала emblib, затем etk с бампом emblib
в том же коммите.

1. **emblib** `feat(canopen)!: build the dictionary at compile time, pass
   handlers a context`
   - изменить: `emb/can/canopen/od.hpp`, `detail/sdo_server.hpp`,
     `server.hpp`;
   - создать: `od_dictionary.hpp`, `detail/od_check.hpp`,
     `od_handlers.hpp`, `emb/test/canopen_od_test.cpp`;
   - докомментарии на уровне классов, в стиле cppreference; `""` для
     соседних файлов, `<>` для путей от корня.
2. **emblib** `feat(canopen): bind settings parameters to the dictionary`
   - создать: `od_settings.hpp`, `emb/test/canopen_od_settings_test.cpp`;
   - обновить `docs/settings-nvm-design.md`: §7 и §8 построены, `expose`
     теперь читается, restore по ключу идёт через `restore_default_at`;
   - обновить `docs/doc-comments-checklist.ru.md` и статус этого
     документа.
3. **etk** `refactor(od): build the dictionary at compile time`, с бампом
   emblib.
   - изменить: `comm/can/canopen/od.hpp`, `od.cpp` (переписать),
     `server.hpp`, `server.cpp`, `main.cpp`;
   - баннеры `// ====` и комментарии-обоснования из `od.cpp` убрать,
     обоснование — в тело коммита.

## 4. Конвертация таблицы etk

Строки переписывает скрипт (perl), а не человек. Руками легко незаметно
переставить обработчики одного типа, например `Tpwr<1>` и `Tpwr<2>` или
`dc_a` и `dc_b`, и ни одна проверка этого не поймает. Скрипт выводит
привязку из старых колонок R и W; ревью — `git diff --word-diff`, где в
каждой строке видно только схлопывание колонок.

Формат строки: `{{I, S}, {"кат", "подкат", "имя", "ед", A, T, D, R, W}}` →
`{{I, S}, "кат", "подкат", "имя", "ед", <привязка>}`.

| Старая строка (число) | Привязка | Что меняется в обработчике |
|---|---|---|
| ro, скаляр (81) | `od_ro<f>` | `od_read_result f()` → `T f()` или `T f(context const&)`; `return od_value{X}` → `return X`; тип писать явно |
| const_, скаляр (6) | `od_const<f>` | то же; серийный номер — `od_const<apm32::f4::core::serial_number>` |
| const_, строка (5) | `od_text<f>` | `char const* f() { return sysinfo::x; }`; счётчики, `string_chunk_reader` и его pragma удалить |
| wo, exec (8) | `od_exec<f>` | параметр `od_value` уходит; синглтон → `context&`; `void`, если команда не может отказать |
| 1011:04 (1) | `od_restore_default` | мёртвый writer удалить |
| wo, float (6) | `od_wo<f>` | `void f(context& c, W v)`, где W — `edeg_f32`, `unsigned_pu_f32`, `hz_f32` или `float`; блок `get_if` удалить |
| rw, настройки (58) | `param::rw<"P">` | P — имя из старого `read_param<"P">`; колонка умолчания, thunk'и, `read_parameter`, `write_parameter` и `to_settings_value` удаляются |

Семейства: `Tpwr<I>` и `Tmot<I>` возвращают
`std::expected<float, sdo_abort_code>`, `Vext<I>` — `float`, `logger_chN`
становится `logger<probe_channel::chN>`.

Замены состояния: `md::motor_drive::instance()->` → `c.drive.`,
`hall::angle_sensor::exists()` → `c.hall_sensor != nullptr`,
`hall::angle_sensor::instance()->` → `c.hall_sensor->`,
`standing_still()` → `standing_still(c)`, `dc_test::x = *p` →
`c.dc_test.x = v`.

## 5. Остальные потребители

Каждый переходит при своём следующем бампе emblib.

- **bike** — как etk: контекст из привода, датчика Холла и `dc_test`
  (глобалы читаются в `server.cpp:292-294`); 157 строк, 57 параметров.
- **psfb** — синглтон `dcdc::converter` в `clear_errors` переезжает в
  контекст; 61 строка, 2 параметра.
- **h2-hess.**
  - `cshpp` привязывает 1011:04 к `od_no_write` — нужен
    `od_restore_default` (T8);
  - фасад настроек `chss`, `cshpp` и `pdu` должен дать аксессор с формой
    `restore_default_at`;
  - в `gpmu` настроек нет, значит нет и каталога.
- **sevpress** (emblib на 203 коммита старше, `emb::nvm`) — отдельная
  миграция. Сейчас его сервер проверяет умолчание раньше права записи и для
  цели без умолчания отвечает `data_store_error` (08000020); после перехода
  он будет отвечать 06010002 или 08000024.

## 6. Проверка

**Тесты emblib.** Только `static_assert`, в безымянном пространстве имён.
Глоб собирает их в прошивку каждого потребителя (ARM GCC 15, `-Werror`);
кроме того, их прогоняют на host g++ 16 и через `clang-check`.

`emb/test/canopen_od_test.cpp`:
- порядок ключей, `sizeof(od_entry<ctx>) == 8 + 2 * sizeof(void*)`;
- access и тип каждого построителя: все 8 скаляров, обёртка, `expected`;
- `od_rw`: чтение и запись через одну запись, читатель с обёрткой и
  писатель со скаляром, негативный концепт на расхождение скаляров;
- негативные концепты с `double`, `std::int64_t` и структурой;
- сортировка, `find` с попаданием и промахом;
- `restorable` и поднятая restore-функция;
- вызовы через записи: чтение обёртки, ошибка из `expected`, писатель,
  получающий обёртку, ошибки команд, слова строки `"abcdefg"`:
  `0x64636261`, `0x00676665`, затем `0`;
- точный текст `detail::od_check(bad)` для T1–T14. `make_dictionary`
  зовёт ту же функцию, так что этим проверены и его отказы.

`emb/test/canopen_od_settings_test.cpp`:
- схема с bool, int32, uint32, float и обёрткой, плюс незаписываемый и
  скрытый параметры; constexpr-заглушки аксессоров;
- `od_type_of`, `to_sdo_abort`, отказ для int8;
- привязки `rw` и `ro`, маппинг ошибок через записи;
- тексты T14, S2 и S3.

**Временная сверка со старой таблицей** (перед коммитом удалить). Кортежи
старых строк извлекаются так:

```
perl -ne 'print "{{$1, $2}, \"$3\", \"$4\", \"$5\", \"$6\", od_access::$7, od_value_type::$8, \"$9\"},\n" if /^\{\{(0x\w+), (0x\w+)\}, \{"([^"]*)", "([^"]*)", "([^"]*)", *"([^"]*)", *od_access::(\w+), *od_value_type::(\w+), *(?:std::nullopt|to_od_value\(settings::parameter<"([^"]+)">)/' od.cpp
```

Это 165 строк `{ключ, категория, подкатегория, имя, единица, access, тип,
параметр}`. Они вставляются как `inline constexpr old_row old_rows[]`, а
`consteval std::string same_as_before()` проверяет для каждой старой
строки, что есть новая с тем же ключом, теми же именами и единицей, тем же
access и типом; что `restorable` стоит ровно там, где был параметр, и
`arg == *settings::schema.index_of(param)`; что строк столько же. Затем
`static_assert(same_as_before().empty(), same_as_before());`. Перестановку
обработчиков одного типа эта сверка не видит — от неё защищает раздел 4.

**Прошивка.**
- Все четыре пресета (`rev-a-release`, `rev-a-debug`, `miniboard-release`,
  `miniboard-debug`) собираются с `-Werror`.
- Строк таблицы и имён в образе нет:
  `arm-none-eabi-nm -C elf | grep -c 'od::rows\|od_ro<\|od_wo<\|od_exec<'`
  и `strings -a elf | grep -c hall_anglesensor` дают 0.
- Ручная проба, без коммита: дубликат ключа и обработчик, возвращающий
  `int`, дают по одной ошибке на строке таблицы.

**Размеры.** Родительский коммит и изменение собираются в двух копиях
дерева с путями одинаковой длины: debug кладёт `__FILE__` в `.rodata`.
Сравниваются секции, а не итог `size`: git-version-tracking кладёт в
образ тему и тело коммита. `arm-none-eabi-size -A` для `.text`, `.data` и
`.bss`; `.rodata` — за вычетом входных секций `git.cpp.obj` по map-файлу;
плюс суммы входных секций `od.cpp.obj`, `server.cpp.obj` и `main.cpp.obj`
и дифф `arm-none-eabi-nm -S -C --size-sort`.

**Стенд** (ucan-monitor, старая и новая прошивка):
1. Прочитать все объекты: значения правдоподобны, `n` верный (bool — 3,
   float — 0).
2. Ошибки чтения: wo-объекты и 1011:04 → 06010001; неизвестный 5000:FF →
   06020000; неподдерживаемый cs на известном объекте → 05040001, на
   неизвестном → 06020000; abort клиента → без ответа; пачка из 20
   запросов → лишние кадры молча теряются.
3. Строковые объекты: все пять читаются целиком; прочитать один наполовину,
   затем другой, затем снова первый — первый начинается с нулевого слова.
4. Запись: параметр настроек в диапазоне → OK, 3000:12 становится true; вне
   диапазона → 06090030; ro, const_ и строковые объекты → 06010002.
5. Exec: save_all стоя → OK и сброс через 2 s, на ходу → 08000022;
   restore_all, erase_all, reset, clear_errors, emergency; calibrate —
   ошибки состояния; save_angle_sensor_config без датчика Холла →
   06060000.
6. wo-float: поправка угла, Vd limit, частота ШИМ (5000:63 следует за ней);
   `dc_a`, `dc_b`, `dc_c` в режиме direct_pwm.
7. 1011:04: изменённый параметр настроек → читается умолчание, изменения
   pending; цель 5000:11 → 06010002; цель 2001:03 → 08000024; неизвестная
   цель → 06020000; ненулевой байт 3 → то же, что с нулевым.

## 7. Размеры

Измерено 29 сентября 2026 года: базовое дерево (app ad60ca8, emblib
927f8d9) и дерево с изменением собраны в двух копиях с путями одинаковой
длины. FLASH — сумма `.apm32_isr_vector`, `.text`, `.rodata`, `.ARM`,
`.init_array`, `.fini_array` и `.data`; вклад `git.cpp.obj` в `.rodata` в
обеих копиях одинаков (139 B).

| Пресет | FLASH до | FLASH после | Δ FLASH | Δ `.text` | Δ `.rodata` | Δ `.data` | Δ `.bss` |
|---|---:|---:|---:|---:|---:|---:|---:|
| rev-a-release | 107 500 | 96 156 | −11 344 | −4 892 | +816 | −7 268 | −24 |
| rev-a-debug | 434 160 | 434 720 | +560 | +6 060 | +1 768 | −7 268 | −24 |
| miniboard-release | 105 804 | 94 412 | −11 392 | −4 932 | +808 | −7 268 | −24 |
| miniboard-debug | 426 164 | 426 992 | +828 | +6 344 | +1 752 | −7 268 | −24 |

Статическая RAM (`.data` + `.bss`) меньше на 7 292 B во всех пресетах:
таблица со span (7 268 B) и static-счётчики строк. Словарь в образе —
2 644 B (165 записей по 16 B и restore-функция).

В release из `.text` ушли `std::sort` с `init_dictionary()` (около 2.7 KB
вместе с конструктором обёртки сервера), 116 thunk'ов настроек с их общими
телами, десять `logger_chN` и строковые читатели. Типизированный thunk при
`-O3` не больше прежнего обработчика: `Vdc` 24 → 24 B, `hall_speed`
52 → 40, `set_pwm_freq` 240 → 232, `save_all_parameters` 184 → 172. В debug
`.text` вырос на 6 KB: при `-O0` на каждую строку приходятся обработчик,
его thunk (`od_read_thunk` — 8 KB на все) и `od_call` (2.7 KB). Рост
перекрыт уходом таблицы из `.data`, и FLASH debug вырос меньше чем на 1 KB.

Имён в образе нет: в образе базы (`objcopy -O binary`) находятся 6 имён из
таблицы (`hall_anglesensor`, `model_controllers`, …), в новом — ни одного,
во всех четырёх пресетах.

`od.cpp` с флагами rev-a-release компилируется 2.20 s и 374 MB против
1.95 s и 362 MB у прежнего файла.

## 8. Ловушки, найденные пробами

- **`throw` в consteval** под `-fno-exceptions` — ошибка компиляции даже в
  недостижимой ветке, на GCC и clang.
- **Где объявлять строки.** Только `inline constexpr` в именованном
  пространстве имён. `constexpr` без `inline`, как и любое объявление в
  безымянном пространстве, при `-O0` уходит в объектный файл вместе с
  именами.
- **Однобайтовые объекты построителей.** При `-O0` каждая использованная
  специализация `od_ro<F>`, `od_wo<F>` и прочих — отдельный 1-байтовый
  объект в `.rodata`. Их выбрасывает `--gc-sections`; проверка образа в
  разделе 6 их ищет.
- **Индексация пустого пака.** `A...[I]` в псевдониме внутри трейтов GCC 15
  и 16 отвергают на пустом паке (`cannot index an empty pack`), clang
  принимает. Аргументы держать как `emb::typelist` и индексировать лениво,
  после проверки арности.
- **`noexcept`.** Специализация `R(*)(A...)` не ловит
  `R(*)(A...) noexcept`. Трейты специализируются по `noexcept(B)`, как
  `detail::delegate_signature` в `delegate.hpp`.
- **`-Werror=address` в тестах.** `static_assert(d.restore() != nullptr)`
  GCC отвергает: адрес функции никогда не null. Сравнивать с ожидаемой
  функцией или проверять `restorable`.
- **`int` и `int32_t`.** Тесты собираются и на ARM, и на x86. На x86
  `int` — это `int32_t`, на ARM (`long`) — нет. Негативные тесты берут
  `double`, `std::int64_t` или структуру.
- **Ошибка до `->`.** В сервере результат чтения проверяется до `assert` и
  до `to_raw`.
- **Каскад ошибок.** Без `diagnosed` ошибка построителя тянет за собой
  вторую из `od_check`.
- **Const контекста мелкий.** `context const&` не защищает `drive` от
  изменения: это член-ссылка.

## 9. Отличия реализации от плана

- `std::ranges::find` по четырём байтам курсора строк тянул в образ
  `memchr` (160 B); курсор проверяет байты через `std::ranges::any_of`.
- Имена обработчиков etk сохранены (`get_device_name`, `get_serial_number`
  и др.), чтобы конвертация таблицы осталась механической; серийный номер
  читает `get_serial_number()`, а не `apm32::f4::core::serial_number`
  напрямую.
- Комментарии `od.cpp`, которые по-прежнему верны (состояние NVM,
  `nvm_erase_cycles`, `save_all_parameters`, `erase_all_parameters`),
  оставлены; ушли баннеры, комментарий о thunk'ах настроек и о глобалах
  `dc_test`.
- Временная сверка со старой таблицей (раздел 6) прошла на копии `od.cpp` и
  в рабочее дерево не попадала; две внесённые в неё ошибки она поймала.
  Тела обработчиков сверены со старыми скриптом: 67 совпали дословно,
  остальные 16 отличаются только ожидаемыми заменами (типизированный
  возврат, контекст вместо синглтона, `void` у команд).
- В тестах emblib массивы строк таблиц огорожены `// clang-format off`, как
  таблица etk.
