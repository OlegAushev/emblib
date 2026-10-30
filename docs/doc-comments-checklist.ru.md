# Документирующие комментарии: чеклист

Какие файлы emblib прошёл агент `cpp-doc-commenter` (`.claude/agents/cpp-doc-commenter.md`
в репозитории adpt-etk-inverter). Отмечен файл, в котором задокументированы все
сущности; у начатых файлов под ним — список сущностей.

Тестовые прогоны 2026-09-24 шли в копиях дерева в песочнице, и их результат в
emblib не попал. Такие файлы помечены «песочница»: сущности там уже разобраны, но
комментарии нужно писать заново.

## Начатые

- [x] `emb/concurrent/double_buffer.hpp`
  - [x] `double_buffer`
- [x] `emb/container/circular_buffer.hpp`
  - [x] `circular_buffer`
- [x] `emb/container/inplace_queue.hpp`
  - [x] `inplace_queue`
- [x] `emb/container/inplace_stack.hpp`
  - [x] `inplace_stack`
- [x] `emb/container/inplace_vector.hpp`
  - [x] `inplace_vector`
- [x] `emb/controller.hpp`
  - [x] `controller_action`
  - [x] `detail::error`
  - [x] `p_controller`
  - [x] `pi_controller_params`
  - [x] `antiwindup::some_scheme`, `antiwindup::backcalculation`,
    `antiwindup::clamping`
  - [x] `pi_controller`
  - [x] `backcalculation_pi_controller`, `clamping_pi_controller`
- [x] `emb/expected.hpp`
  - [x] `EMB_TRY`
- [x] `emb/filter/cascaded_filter.hpp`
  - [x] `cascaded_filter`
- [x] `emb/filter/concepts.hpp`
  - [x] `some_filter`
- [x] `emb/filter/exponential_filter.hpp`
  - [x] `exponential_filter`
- [x] `emb/filter/median_filter.hpp`
  - [x] `median_filter`
- [x] `emb/filter/moving_average_filter.hpp`
  - [x] `moving_average_filter`
- [x] `emb/filter/passthrough_filter.hpp`
  - [x] `passthrough_filter`
- [x] `emb/foc/clarke.hpp`
  - [x] `clarke_inputs`
  - [x] `clarke_transform`
  - [x] `invclarke_transform`
- [x] `emb/i18n.hpp`
  - [x] `text`
  - [x] `language`, `languages`
  - [x] `is_complete`
- [x] `emb/math.hpp` — только подключает заголовки `emb/math/`, сущностей нет
- [x] `emb/math/angle.hpp`
  - [x] `to_rad`, `to_deg`, `to_eradps`, `to_rpm`
  - [x] `norm2pi`, `normpi`, `norm2pi_fast`, `normpi_fast`
- [x] `emb/math/clamped.hpp`
  - [x] `clamped` (и друг `unit_symbol_of` для `signed_pu`/`unsigned_pu`)
  - [x] `signed_pu`, `unsigned_pu`, `signed_pu_f32`, `unsigned_pu_f32`
- [x] `emb/math/numeric.hpp`
  - [x] `fmod_trivial`, `fmod`
  - [x] `sgn`, `iseven`, `isodd`, `approx`
- [x] `emb/math/scaled.hpp`
  - [x] `saturate_round`
  - [x] `quantize`, `dequantize`
  - [x] `some_ratio`
  - [x] `detail::scaled`
  - [x] `scaled` (псевдоним)
- [x] `emb/math/sqrt.hpp`
  - [x] `fast_rsqrt`
  - [x] `builtin_rsqrt`, `rsqrt`
  - [x] `fast_sqrt`
  - [x] `builtin_sqrt`, `sqrt`
- [ ] `emb/math/trigonometric.hpp` — песочница: `lookup_sin`, `fast_atan2`, таблица sin/cos
  - [ ] `detail::sincos_lookup_table`
  - [ ] `lookup_sin`, `lookup_cos`
  - [ ] `fast_atan2`
  - [x] `builtin_sin`, `sin`
  - [x] `builtin_cos`, `cos`
  - [x] `builtin_atan2`, `atan2`
- [x] `emb/meta.hpp` — только подключает заголовки `emb/meta/`, сущностей нет
- [x] `emb/meta/all_same.hpp`
  - [x] `all_same` (`detail::all_same_v` без комментария: его покрывает
    концепт)
- [x] `emb/meta/alternative_of.hpp`
  - [x] `is_alternative_of`, `is_alternative_of_v`, `alternative_of`
- [ ] `emb/meta/enum_carrier32.hpp`
  - [ ] `enum_carrier32_t` (и `detail::fits_carrier32`) — вручную 2026-10-22
- [ ] `emb/meta/enum_info.hpp`
  - [ ] `enumerator_info`, `enum_with_info`, `enum_info_of` — вручную 2026-10-22
- [x] `emb/meta/fixed_string.hpp`
  - [x] `fixed_string` (правило вывода и `detail::concat_chars` без
    комментариев)
  - [x] `operator+` (три перегрузки, у каждой свой комментарий)
- [x] `emb/meta/nth_type.hpp`
  - [x] `nth_type`, `nth_type_t`
- [x] `emb/meta/overload.hpp`
  - [x] `overload`
- [x] `emb/meta/replicate.hpp`
  - [x] `detail::replicated`, `replicate`, `replicate_t`
- [x] `emb/meta/same_as_any.hpp`
  - [x] `same_as_any`
- [x] `emb/meta/type_index.hpp`
  - [x] `type_index`, `type_index_v`
- [x] `emb/meta/typelist.hpp`
  - [x] `typelist`
  - [x] `is_typelist`, `is_typelist_v`, `some_typelist`
  - [x] `typelist_size`, `typelist_size_v`
  - [x] `typelist_contains_t`, `typelist_contains_v`, `typelist_contains`
  - [x] `typelist_at`, `typelist_at_t`
  - [x] `typelist_count_t`, `typelist_count_v`
  - [x] `typelist_unique_t`, `typelist_unique_v`, `typelist_unique`
  - [x] `typelist_append`, `typelist_append_t`
  - [x] `typelist_concat`, `typelist_concat_t`
- [x] `emb/meta/unit_symbol.hpp`
  - [x] `unit_with_symbol`, `unit_symbol_of`
- [x] `emb/meta/unroll.hpp`
  - [x] `unroll`
- [x] `emb/meta/visit_at.hpp`
  - [x] `visit_at`
- [x] `emb/meta/wrapper.hpp`
  - [x] `scalar_wrapper`, `unwrapped_t` (`detail::unwrapped` без комментария:
    его покрывает псевдоним)
- [x] `emb/mmio.hpp`
  - [x] `some_register`, `some_writable_register`, `mask_type`, `value_type`
  - [x] `detail::is_contiguous_mask`, `valid_mask`, `field_mask`, `mask_for`,
    `field_mask_for`, `flag_mask_for`
  - [x] `read`, `write` (`runtime::` и шаблонные)
  - [x] `set`, `clear`, `toggle`, `set_or_clear` (`runtime::` и шаблонные)
  - [x] `test_any`, `test_all`, `test` (`runtime::` и шаблонные)
  - [x] `clear_w1`, `clear_w0` (`runtime::` и шаблонные)
  - [x] `bits`, `modify`
- [ ] `emb/panic.hpp` (бывший `emb/assert.hpp`)
  - [x] `emb::panic` (бывший `emb::ensure_failed`)
  - [x] `emb::ensure` (устаревший)
  - [ ] `EMB_PANIC` (и шаблон `emb::panic<Message>`) — вручную
    2026-10-23
  - [ ] `EMB_PRE` (вместо `ASSUME`) — вручную 2026-10-24
- [x] `emb/settings/image.hpp`
  - [x] `error`
  - [x] `image`
- [x] `emb/settings/param.hpp`
  - [x] `apply_policy` (и `enum_info_of(apply_policy)`, вручную 2026-10-22)
  - [x] `descriptor`
  - [x] `detail::lowest`, `detail::highest`, `detail::identify`,
    `detail::labelled_between` (вручную 2026-10-22)
  - [x] `options` (и поля `label`, `unit`), `declaration` (и поля `label`,
    `unit`), `param`
- [x] `emb/settings/record.hpp`
  - [x] `record_format` (с описанием формата записи)
  - [x] `record_header_size`, `record_cell_size`, `record_footer_size`
  - [x] `record_body_size`, `record_size`
  - [x] `record_header`
  - [x] `seq_newer`, `schema_id`
  - [x] `load_report`
  - [x] `detail::put_u16`, `detail::put_u32`, `detail::get_u16`,
    `detail::get_u32`
  - [x] `detail::crc32`
  - [x] `encode_record`, `decode_header`, `check_record`, `decode_record`
- [ ] `emb/settings/rule.hpp`
  - [ ] `inputs`, `detail::declaration_tag`, `detail::same_declaration` —
    комментарии написаны вручную 2026-10-17
  - [ ] `some_rule`, `detail::is_inputs_of`, `all_hold` — вручную 2026-10-17
- [x] `emb/settings/schema.hpp`
  - [x] `basic_schema`
  - [x] `make_schema`
  - [x] `schema_t`, `type_at`
  - [x] `parameter`
- [ ] `emb/settings/section.hpp`
  - [x] `section` — комментарии членов применения, отбрасывания и образов
    написаны вручную 2026-10-16, применения и отката по правилам —
    2026-10-17
  - [ ] `apply_refusal`, `save_refusal`, `section_save_failure` — вручную
    2026-10-16 и 2026-10-17
  - [ ] `some_checker`, `some_applier` — вручную 2026-10-16, переписаны
    2026-10-17
- [x] `emb/settings/store.hpp`
  - [x] `placement`
  - [x] `save_stage`, `save_failure`, `save_result`, `load_result`
  - [x] `detail::slot_ring`
  - [x] `store`
- [x] `emb/settings/value.hpp`
  - [x] `scalar_kind`
  - [x] `value`, `cell`
  - [x] `some_value`, `some_wrapped_value`, `some_parameter_type`,
    `some_enum_value` (вручную 2026-10-22)
  - [x] `scalar_t` (и `detail::scalar` без комментария: его покрывает
    псевдоним), `kind_of`
  - [x] `held_kind`
  - [x] `to_value`, `from_value`, `to_cell`, `from_cell` (у `to_value` и
    `to_cell` по две перегрузки, у каждой свой комментарий)
  - [x] `less_equal`, `in_range`
- [x] `emb/signal.hpp` — только подключает заголовки `emb/signal/`, сущностей нет
- [x] `emb/signal/affine.hpp`
  - [x] `affine`
- [x] `emb/signal/bind.hpp`
  - [x] `bind`
- [x] `emb/signal/path.hpp`
  - [x] `detail::path_forward`, `detail::path_inverse`
  - [x] `path`
- [x] `emb/signal/proportional.hpp`
  - [x] `proportional`
- [x] `emb/signal/sign.hpp`
  - [x] `sign_reversible`
  - [x] `identity`, `negation`
- [x] `emb/singleton.hpp`
  - [x] `singleton`
  - [x] `singleton_array`
- [x] `emb/units.hpp` — только подключает заголовки `emb/units/`, сущностей нет;
  у каждого тега `tags::*` в заголовках ниже — член `symbol`, его называет
  комментарий тега
- [x] `emb/units/angle.hpp`
  - [x] `tags::erad`, `tags::edeg`, `tags::rad`, `tags::deg`
  - [x] `erad`, `edeg`, `rad`, `deg`, `erad_f32`, `edeg_f32`, `rad_f32`,
    `deg_f32`
  - [x] `quantity_of_electrical_angle`, `quantity_of_angle`,
    `quantity_in_radians`, `quantity_in_degrees`
  - [x] `convert_to` (четыре перегрузки)
  - [x] `norm2pi`, `normpi`, `norm2pi_fast`, `normpi_fast`
  - [x] `norm360`, `norm180`, `norm360_fast`, `norm180_fast`
- [x] `emb/units/chrono.hpp`
  - [x] `tags::hz`, `tags::sec`
  - [x] `hz`, `sec`, `hz_f32`, `sec_f32`
  - [x] `operator/` (две перегрузки)
  - [x] `to_duration`, `to_milliseconds`
- [x] `emb/units/conductivity.hpp`
  - [x] `tags::microsiemens_per_cm`, `microsiemens_per_cm`,
    `microsiemens_per_cm_f32`
- [x] `emb/units/electrical.hpp`
  - [x] `tags::amp`, `tags::volt`, `tags::ohm`
  - [x] `amp`, `volt`, `ohm`, `amp_f32`, `volt_f32`, `ohm_f32`
- [x] `emb/units/flow.hpp`
  - [x] `tags::cubic_meter_per_hour`, `tags::liter_per_minute`
  - [x] `cubic_meter_per_hour`, `liter_per_minute`, `cubic_meter_per_hour_f32`,
    `liter_per_minute_f32`
  - [x] `convert_to` (две перегрузки)
- [x] `emb/units/pressure.hpp`
  - [x] `tags::pascal`, `tags::megapascal`, `tags::atmosphere`
  - [x] `pascal`, `megapascal`, `atmosphere`, `pascal_f32`, `megapascal_f32`,
    `atmosphere_f32`
  - [x] `convert_to` (шесть перегрузок)
- [x] `emb/units/quantity.hpp`
  - [x] `quantity` (и друг `unit_symbol_of`), `some_quantity`, `convert_to`
- [x] `emb/units/relations.hpp`
  - [x] `operator/`
  - [x] `operator*` (две перегрузки)
- [x] `emb/units/speed.hpp`
  - [x] `tags::rpm`, `tags::eradps`
  - [x] `rpm`, `eradps`, `rpm_f32`, `eradps_f32`
  - [x] `quantity_of_rotational_speed`
  - [x] `convert_to` (две перегрузки)
- [x] `emb/units/temperature.hpp`
  - [x] `tags::degree_celsius`, `tags::kelvin`
  - [x] `degree_celsius`, `kelvin`, `degree_celsius_f32`, `kelvin_f32`
  - [x] `convert_to` (две перегрузки)

## Остальные

- [ ] `emb/actuator.hpp`
- [ ] `emb/actuator/concepts.hpp`
- [ ] `emb/actuator/discrete.hpp`
- [ ] `emb/actuator/feedback.hpp`
- [ ] `emb/actuator/monitored.hpp`
- [ ] `emb/actuator/unmonitored.hpp`
- [ ] `emb/algorithm.hpp` — песочница: `median3`
- [ ] `emb/can.hpp`
- [ ] `emb/can/bus.hpp`
- [ ] `emb/can/canopen/detail/emcy_producer.hpp`
- [ ] `emb/can/canopen/detail/hb_consumer.cpp`
- [ ] `emb/can/canopen/detail/hb_consumer.hpp`
- [ ] `emb/can/canopen/detail/hb_producer.hpp`
- [ ] `emb/can/canopen/detail/nmt_slave.hpp`
- [ ] `emb/can/canopen/detail/od_check.hpp`
- [ ] `emb/can/canopen/detail/od_describe.hpp`
- [ ] `emb/can/canopen/detail/pdo_check.hpp`
- [ ] `emb/can/canopen/detail/rpdo_consumer.hpp`
- [ ] `emb/can/canopen/detail/sdo_server.hpp`
- [ ] `emb/can/canopen/detail/sync_producer.hpp`
- [ ] `emb/can/canopen/detail/tpdo_producer.hpp`
- [ ] `emb/can/canopen/mpdo.hpp`
- [ ] `emb/can/canopen/od.hpp` — песочница: `od_access`, `od_scalar`
- [ ] `emb/can/canopen/od_description.hpp`
- [ ] `emb/can/canopen/od_dictionary.hpp`
- [ ] `emb/can/canopen/od_handlers.hpp`
- [ ] `emb/can/canopen/od_process_data.hpp`
- [ ] `emb/can/canopen/od_section.hpp`
- [ ] `emb/can/canopen/od_settings.hpp`
- [ ] `emb/can/canopen/od_trouble.hpp`
- [ ] `emb/can/canopen/od_value_cast.hpp`
- [ ] `emb/can/canopen/pdo.hpp`
- [ ] `emb/can/canopen/sdo.hpp`
- [ ] `emb/can/canopen/server.hpp`
- [ ] `emb/can/canopen/types.hpp`
- [ ] `emb/can/node_link.hpp`
- [ ] `emb/can/raw/node.hpp`
- [ ] `emb/can/rolling_counter.hpp`
- [ ] `emb/chrono.hpp`
- [ ] `emb/concurrent/latched_seqlock.hpp`
- [ ] `emb/concurrent/memory_scope.hpp`
- [ ] `emb/concurrent/seqlock.hpp`
- [ ] `emb/concurrent/spmc_buffer.hpp`
- [ ] `emb/concurrent/spsc_queue.hpp`
- [ ] `emb/concurrent/triple_buffer.hpp`
- [ ] `emb/concurrent/wide_counter.hpp`
- [ ] `emb/delegate.hpp`
- [ ] `emb/foc.hpp`
- [ ] `emb/foc/deadtime_compensation.hpp`
- [ ] `emb/foc/dq_controller.hpp`
- [ ] `emb/foc/park.hpp`
- [ ] `emb/foc/pwm.hpp`
- [ ] `emb/foc/to_polar.hpp`
- [ ] `emb/foc/types.hpp`
- [ ] `emb/foc/utility.hpp`
- [ ] `emb/fsm/command.hpp`
- [ ] `emb/fsm/fsm_sp_cpp23.hpp`
- [ ] `emb/fsm/fsm_v2.hpp`
- [ ] `emb/fsm/fsm_v3.hpp`
- [ ] `emb/functional.hpp`
- [ ] `emb/generator/ramp_generator.hpp`
- [ ] `emb/generator/sine_generator.hpp`
- [ ] `emb/gpio.hpp`
- [ ] `emb/hall.hpp`
- [ ] `emb/hall/angle_sensor.hpp`
- [ ] `emb/hall/calibration.cpp`
- [ ] `emb/hall/calibration.hpp`
- [ ] `emb/hall/emulator.cpp`
- [ ] `emb/hall/emulator.hpp`
- [ ] `emb/hall/error.hpp`
- [ ] `emb/hall/sector.hpp`
- [ ] `emb/integrator.hpp` — песочница: `integrator`
- [ ] `emb/math/saturation.hpp`
- [ ] `emb/noncopyable.hpp`
- [ ] `emb/nvm/storage.hpp`
- [ ] `emb/pipe.hpp`
- [ ] `emb/scheduler/basic_scheduler.hpp`
- [ ] `emb/scope.hpp`
- [ ] `emb/sensor.hpp`
- [ ] `emb/sensor/buffered.hpp`
- [ ] `emb/sensor/concepts.hpp`
- [ ] `emb/sensor/multichannel.hpp`
- [ ] `emb/sensor/multiplexed.hpp`
- [ ] `emb/sensor/signalpath.hpp`
- [ ] `emb/sensor/singlechannel.hpp`
- [ ] `emb/spi.hpp`
- [ ] `emb/three_phase.hpp`
- [ ] `emb/trouble.hpp`
