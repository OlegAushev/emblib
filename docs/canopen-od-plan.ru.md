# Словарь объектов CANopen: constexpr-таблица и контекст обработчиков — план

> Исторический документ: план перехода сентября 2026 года, сохранённый ради
> обоснований и отвергнутых вариантов. С тех пор словарь изменился: 16
> октября 1011h:04 удалён, словарь научился составным объектам, а параметры
> и группы настроек стали ими; 17 октября группы ушли, а с ними ссылки и
> второй аргумент составных объектов; 19 октября из `od_data_type` ушли
> `exec` и `string`: команда стала объектом `uint32` только для записи, а
> текст — массивом символов. Как словарь устроен сейчас, описывает
> `canopen-od-algorithm.ru.md`.

Статус: сделано 29 сентября 2026 года. Отличия реализации от плана — в
разделе 7, измеренные размеры — в разделе 5.

План составлен 28–29 сентября 2026 года с нуля. Предыдущая версия этого
документа (emblib 927f8d9) при этом не использовалась; этот текст её
заменяет. Каждая C++-конструкция ниже проверена пробами на
arm-none-eabi-g++ 15.2 с флагами приложения, включая `-Werror`, и на host
g++ 16.2; большинство — также на clang 22 через `clang-check`.

Что меняется. Таблица словаря становится constexpr-объектом: строки
сортируются и проверяются при компиляции, образ хранит 16-байтовые записи в
`.rodata`, а имена и единицы остаются только в исходнике. Обработчики
типизированы: тип объекта выводится из сигнатуры обработчика, контекст
приложения приходит по ссылке. Строки параметров настроек строятся из
схемы. API emblib ломается; на шине всё по-прежнему, кроме одного случая со
строковыми объектами (§2.7).

## Решения

- 2026-09-28. Обработчики типизированные (§2.4). Отвергнутый вариант —
  сырые обработчики `(Ctx&, arg, od_value)` с явной колонкой типа: в нём
  совпадение типа строки и обработчика проверял бы только debug-`assert`.
- 2026-09-28. 0x1011:04 обслуживает сервер, как сейчас (§2.6). Отвергнутый
  вариант — обычная строка приложения: её обработчику нужен поиск по
  словарю, в котором она сама лежит, а часть приложений поменяла бы
  поведение на шине.
- 2026-09-28. У строковых объектов один курсор на сервер (§2.6).
- 2026-09-29. Построитель `od_rw<G, S>` входит в набор, хотя в приложении
  ему пока нечего обслуживать.
- 2026-09-29. В строке таблицы записан тип объекта (`od_data_type`), чтобы
  таблицу для клиента на ПК мог строить текстовый парсер; `make_dictionary`
  сверяет его с типом привязки (T15). Тип — колонкой строки, а не
  аргументом построителя: так он стоит на одном месте в каждой строке, а
  проверка одна на все привязки. Доступ по-прежнему называет построитель.
  Пределы и умолчания в строку не пишутся: в схеме это выражения, которые
  текстовый парсер не вычислит.

## 1. Что сейчас

- `emb/can/canopen/od.hpp`: строка — это `od_entry {od_key, od_object}`. В
  `od_object` четыре `char const*`, access, тип, `std::optional<od_value>`
  умолчания и два указателя, `read()` и `write(od_value)`, которым некуда
  передать контекст. На ARM строка занимает 44 B.
- Таблица приложения (165 строк) — неконстантный `std::array`, потому что
  `sdo_server` сортирует её `std::sort` при старте. Поэтому она лежит в
  `.data`: 7 260 B RAM и столько же flash под образ инициализации.
  Дубликаты ключей ловит `assert`, то есть только в debug.
- Обработчики берут состояние из синглтонов, из extern-глобалов и из
  static-счётчиков пяти строковых объектов. Каждому из 58 параметров
  настроек нужна своя пара thunk'ов чтения и записи.
- 0x1011:04 (восстановить умолчание одного параметра): сервер перехватывает
  запись до поиска и пишет в строку-цель её `default_value`. Строка
  0x1011:04 в таблице приложения есть, но её writer никто не вызывает.
- Исходник таблицы никто не разбирает — ни скрипты, ни codegen; клиент
  на ПК держит собственную рукописную копию метаданных.

## 2. Дизайн

### 2.1 Типы (`emb/can/canopen/od.hpp`, переписывается)

Остаются `od_value`, `od_scalar`, `od_data_type`, `od_access`,
`od_read_result`, `od_write_result`, `from_sdo_data`, `to_sdo_data` и
`od_size_of`. Добавляются `static_assert`, связывающие коды 0–7
`od_data_type` с индексами альтернатив `od_value`, и
`constexpr od_alternative_of(od_data_type)`: для `exec` и `string` он
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
  od_data_type type;
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
  od_data_type type;                   // для текстового парсера; T15
  od_binding<Ctx> binding;
};

// Что от строки остаётся в образе: 16 B на Cortex-M4.
template<typename Ctx>
struct od_entry {
  std::uint16_t index;
  std::uint8_t subindex;
  od_data_type type;
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
вроде `sensor<I>` остаются шаблонами, как сейчас.

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
принимают). Строка таблицы в нём выглядит как `3002h:01 config/group/name`.
Диагностика GCC занимает несколько строк, среди них «required from here» на
строке `make_dictionary<rows>()` в приложении.

Сначала идут проверки отдельных строк в порядке исходника, затем проверки
пар, затем каталогов. Если у какой-то строки стоит `diagnosed`, `od_check`
сразу возвращает пустую строку: ошибка построителя (§2.4, §2.5) остаётся
единственной. Без этого за ней следует вторая, сбивающая с толку: для
неизвестного имени параметра, например, виноватым оказывается параметр 0.

| # | Правило | Сообщение |
|---|---|---|
| T1 | index ≥ 1000h | `od: <row>: indices below 1000h are reserved` |
| T2 | категория, подкатегория и имя не пусты | `od: <row>: category, subcategory and name must not be empty` |
| T15 | тип в строке равен типу привязки | `od: <row>: declared <type>, bound as <type>` |
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

T13 ловит скопированную строку вроде второго `sensor<0>`. Пустые обработчики
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
- `sensor<I>` за пределом установленных датчиков (`object_not_found`, как
  сейчас);
- единицы измерения и копия метаданных в клиенте на ПК;
- перепутанные местами обработчики одного типа, например `sensor<1>` и
  `sensor<2>`.

Отвергнуто:
- `throw` в consteval: под `-fno-exceptions` это ошибка компиляции даже в
  недостижимой ветке;
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
`units::quantity`, `emb::clamped`); тип объекта — скаляр T. Построители —
это переменные-шаблоны с `consteval operator od_binding<Ctx>()`, так что
`Ctx` выводится из `od_row<context>`. Thunk'и `read_thunk`, `write_thunk`,
`command_thunk` и `text_thunk` — `constexpr`, чтобы тесты могли звать их
через записи. `text_thunk` отдаёт байты `[4·word, 4·word + 4)` строки,
нулями за её концом.

`od_rw` нужен для rw-объектов вне настроек. restore у него нет:
восстановление умолчаний остаётся за мостом настроек. В приложении таких
строк пока нет (все 58 rw-строк — настройки), так что `od_rw` проверяют
только тесты emblib.

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

- Тип объекта берётся из типа параметра (`od_type_of<settings::scalar_t<T>>`).
- Ошибки аксессоров маппятся так же, как сейчас в приложении:
  `unknown_parameter` → `object_not_found`, `read_only` →
  `write_to_read_only`, `type_mismatch` → `data_type_mismatch`,
  `out_of_range` → `value_range_exceeded`.
- Значение, которого нет среди альтернатив `settings::scalar_variant`
  (узкие целые), даёт `data_type_mismatch`.
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
         data = to_sdo_data(*v)
         string: word = (в data есть 0) ? 0 : word + 1
         n = (4 - od_size_of(e.type)) & 3

write:   !od_writable(e.access)         -> write_to_read_only
         e.write(ctx_, e.arg, from_sdo_data(data, e.type))

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

Относительно нынешней emblib на шине ничего не меняется: коды 06010001,
06010002, 06020000, 05040001 и 08000024, порядок проверок 1011h:04,
отсутствие ответа на abort клиента, молчаливый сброс кадров при полной
очереди, `n` по объявленному типу, нулевые данные в ответе на запись. Для
строк 'abcd', '' и 'abcdefg' байты совпадают с нынешними.

Меняется одно. Если клиент прервёт чтение строкового объекта на середине и
начнёт читать другой, первый потом читается с начала. Сейчас его счётчик
застревает на середине, и следующее чтение даёт мусор. Клиент на ПК читает
каждую строку до нуля и строки не перемежает.

## 3. План работ

1. **emblib** `feat(canopen)!: build the dictionary at compile time, pass
   handlers a context`
   - изменить: `emb/can/canopen/od.hpp`, `detail/sdo_server.hpp`,
     `server.hpp`;
   - создать: `od_dictionary.hpp`, `detail/od_check.hpp`,
     `od_handlers.hpp`, `emb/can/canopen/test/od_test.cpp`;
   - докомментарии на уровне классов, в стиле cppreference; `""` для
     соседних файлов, `<>` для путей от корня.
2. **emblib** `feat(canopen): bind settings parameters to the dictionary`
   - создать: `od_settings.hpp`,
     `emb/can/canopen/test/od_settings_test.cpp`;
   - обновить `docs/settings-nvm-design.md`: §7 и §8 построены, `expose`
     теперь читается, restore по ключу идёт через `restore_default_at`;
   - обновить `docs/doc-comments-checklist.ru.md` и статус этого
     документа.

## 4. Проверка

**Тесты emblib.** Только `static_assert`, в безымянном пространстве имён.
Глоб собирает их в прошивку каждого потребителя (ARM GCC 15, `-Werror`);
кроме того, их прогоняют на host g++ 16 и через `clang-check`.

`emb/can/canopen/test/od_test.cpp`:
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

`emb/can/canopen/test/od_settings_test.cpp`:
- схема с bool, int32, uint32, float и обёрткой, плюс незаписываемый и
  скрытый параметры; constexpr-заглушки аксессоров;
- `od_type_of`, `to_sdo_abort`, отказ для int8;
- привязки `rw` и `ro`, маппинг ошибок через записи;
- тексты T14, S2 и S3.

**Приложение.**
- Release- и debug-сборки собираются с `-Werror`.
- Строк таблицы и имён в образе нет:
  `arm-none-eabi-nm -C elf | grep -c 'od::rows\|od_ro<\|od_wo<\|od_exec<'`
  и `strings -a elf | grep -c` с именем из таблицы дают 0.
- Ручная проба, без коммита: дубликат ключа и обработчик, возвращающий
  `int`, дают по одной ошибке на строке таблицы.

**Размеры.** Родительский коммит и изменение собираются в двух копиях
дерева с путями одинаковой длины: debug кладёт `__FILE__` в `.rodata`.
Сравниваются секции, а не итог `size`: сборка кладёт в образ тему и тело
коммита. `arm-none-eabi-size -A` для `.text`, `.data` и `.bss`; `.rodata` —
за вычетом входных секций с данными коммита по map-файлу; плюс суммы
входных секций файлов таблицы, обёртки сервера и `main` и дифф
`arm-none-eabi-nm -S -C --size-sort`.

## 5. Размеры

Измерено 29 сентября 2026 года: базовое дерево приложения (emblib 927f8d9)
и дерево с изменением собраны в двух копиях с путями одинаковой длины.
FLASH — сумма таблицы векторов, `.text`, `.rodata`, `.ARM`, `.init_array`,
`.fini_array` и `.data`; вклад данных коммита в `.rodata` в обеих копиях
одинаков (139 B).

| Сборка | FLASH до | FLASH после | Δ FLASH | Δ `.text` | Δ `.rodata` | Δ `.data` | Δ `.bss` |
|---|---:|---:|---:|---:|---:|---:|---:|
| release (`-O3`) | 107 500 | 96 156 | −11 344 | −4 892 | +816 | −7 268 | −24 |
| debug (`-O0`) | 434 160 | 434 720 | +560 | +6 060 | +1 768 | −7 268 | −24 |

Статическая RAM (`.data` + `.bss`) меньше на 7 292 B в обеих сборках:
таблица со span (7 268 B) и static-счётчики строк. Словарь в образе —
2 644 B (165 записей по 16 B и restore-функция).

В release из `.text` ушли `std::sort` с `init_dictionary()` (около 2.7 KB
вместе с конструктором обёртки сервера), 116 thunk'ов настроек с их общими
телами и строковые читатели. Типизированный thunk при `-O3` не больше
прежнего обработчика: читатели 24 → 24 B и 52 → 40, писатель 240 → 232,
команда 184 → 172. В debug `.text` вырос на 6 KB: при `-O0` на каждую
строку приходятся обработчик, его thunk (`od_read_thunk` — 8 KB на все) и
`od_call` (2.7 KB). Рост перекрыт уходом таблицы из `.data`, и FLASH debug
вырос меньше чем на 1 KB.

Имён в образе нет: в образе базы (`objcopy -O binary`) находятся 6 имён из
таблицы, в новом — ни одного, в обеих сборках.

Файл таблицы с флагами release компилируется 2.20 s и 374 MB против 1.95 s
и 362 MB у прежнего.

## 6. Ловушки, найденные пробами

- **`throw` в consteval** под `-fno-exceptions` — ошибка компиляции даже в
  недостижимой ветке, на GCC и clang.
- **Где объявлять строки.** Только `inline constexpr` в именованном
  пространстве имён. `constexpr` без `inline`, как и любое объявление в
  безымянном пространстве, при `-O0` уходит в объектный файл вместе с
  именами.
- **Однобайтовые объекты построителей.** При `-O0` каждая использованная
  специализация `od_ro<F>`, `od_wo<F>` и прочих — отдельный 1-байтовый
  объект в `.rodata`. Их выбрасывает `--gc-sections`; проверка образа в
  разделе 4 их ищет.
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
  до `to_sdo_data`.
- **Каскад ошибок.** Без `diagnosed` ошибка построителя тянет за собой
  вторую из `od_check`.
- **Const контекста мелкий.** `context const&` не защищает от изменения
  то, на что ссылается член-ссылка.

## 7. Отличия реализации от плана

- `std::ranges::find` по четырём байтам курсора строк тянул в образ
  `memchr` (160 B); курсор проверяет байты через `std::ranges::any_of`.
- В тестах emblib массивы строк таблиц огорожены `// clang-format off`.
- Уже после реализации, 29 сентября, в строку добавлена колонка типа с
  проверкой T15 (см. «Решения»). Размеры секций образа не изменились. Типы
  в приложении пишутся коротко после `using enum od_data_type;`.
- 15 октября строка стала называть параметр настроек его объявлением, а
  не именем: `param::rw<params::sensor::samples>` вместо
  `param::rw<"sensor.samples">`, как в разделе 2. Объявление —
  `constexpr`-переменная, которую перечисляет схема; `settings::parameter`
  сверяет с ним тип и дескриптор параметра, а имя осталось ключом
  сохранённого значения и каталога. Почему — в журнале решений
  `settings-nvm-design.md`. Образ приложения в release не изменился ни на
  байт.
