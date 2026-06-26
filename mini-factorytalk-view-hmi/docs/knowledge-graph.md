# Knowledge Graph — mini-factorytalk-view-hmi

## L1: Definitions (Complete ✅)

| ID | Knowledge Point | C Implementation | Lean Formalization |
|----|----------------|------------------|-------------------|
| L1.1 | HMI tag data types (analog/digital/integer/string) | `ftview_type_t`, `ftview_value_t` in ftview_common.h | — |
| L1.2 | ISA-101 HMI hierarchy levels (1-4) | `isa101_level_t`, `isa101_display_t` in ftview_common.h | `IsaLevel` inductive type in ftview_formal.lean |
| L1.3 | OPC UA quality-of-service flags | `ftview_quality_t` in ftview_common.h | — |
| L1.4 | HMI object role types (ISA-101 Annex A) | `ftview_role_t` in ftview_common.h | — |
| L1.5 | Display resolution classes | `ftview_resolution_t`, `ftview_display_geometry_t` | — |
| L1.6 | HMI Tag definition (name, type, scaling) | `ftview_tag_t` in ftview_tag.h | — |
| L1.7 | Direct Reference Tag (DRT) format | `ftview_tag_parse_drt()` in ftview_tag.c | — |
| L1.8 | Alarm definition (condition, severity, message) | `ftview_alarm_def_t` in ftview_alarm.h | — |
| L1.9 | Alarm severity levels (ISA-18.2 / EEMUA 191) | `ftview_alarm_severity_t` in ftview_alarm.h | `AlarmSeverity` inductive + total order theorems |
| L1.10 | User account definition | `ftview_user_t` in ftview_security.h | — |
| L1.11 | Security privilege codes (A-O) | `ftview_privilege_t` in ftview_security.h | `PrivilegeMask` + bitwise theorems |
| L1.12 | HMI display object types (12 types) | `ftview_object_type_t` in ftview_graphics.h | — |
| L1.13 | Display screen definition | `ftview_display_screen_t` in ftview_graphics.h | — |
| L1.14 | Data Log Model definition | `ftview_datalog_model_t` in ftview_datalog.h | — |
| L1.15 | Trend configuration (pens, time span) | `ftview_trend_config_t` in ftview_datalog.h | — |

## L2: Core Concepts (Complete ✅)

| ID | Knowledge Point | Implementation |
|----|----------------|---------------|
| L2.1 | Update rate and data freshness | `ftview_tag_is_stale()` in ftview_tag.c |
| L2.2 | Tag resolution (HMI tags vs DRT) | `ftview_tag_parse_drt()`, `ftview_tagdb_resolve()` |
| L2.3 | Tag scaling (engineering unit conversion) | `ftview_tag_scale_raw_to_eu()` — linear + sqrt |
| L2.4 | ISA-18.2 alarm state machine | `ftview_alarm_evaluate()` — full FSM transitions |
| L2.5 | Alarm suppression (shelving, OOS) | `ftview_alarm_shelve()`, `_out_of_service()` |
| L2.6 | Role-Based Access Control (RBAC) | `ftview_security_check_privilege()` |
| L2.7 | Authentication flow (login/session/logout) | `ftview_security_login()`, `_logout()` |
| L2.8 | Object-property binding (expression → tag) | `ftview_property_binding_t`, `ftview_display_bind_property()` |
| L2.9 | Animation types (visibility, colour, position, fill) | `ftview_animation_t`, `ftview_animation_evaluate()` |
| L2.10 | Circular-buffer data logging | `ftview_datalog_sample()` with loss notify |
| L2.11 | Trigger types (periodic, on-change, on-condition) | `ftview_datalog_eval_trigger()` |
| L2.12 | OPC UA client-server lifecycle | `ftview_comm_connect()`, `_read()`, `_write()` |
| L2.13 | RSLinx topic-based routing | `ftview_comm_add_topic()`, `ftview_comm_resolve_drt()` |

## L3: Engineering Structures (Complete ✅)

| ID | Knowledge Point | Implementation |
|----|----------------|---------------|
| L3.1 | Hash-table tag database (FNV-1a, open chaining) | `ftview_tagdb_add()`, `_find()`, `_remove()` |
| L3.2 | Tag subscription model | `ftview_tag_t.subscribed` flag + update_rate |
| L3.3 | Priority-sorted alarm queue | `ftview_alarm_queue_entry_t` + enqueue/dequeue |
| L3.4 | Ring-buffer alarm log | `ftview_alarm_log_entry_t` + circular buffer |
| L3.5 | PBKDF2-HMAC-SHA256 password hashing | `ftview_security_hash_password()` with SHA-256 |
| L3.6 | Audit trail (event type, user, timestamp) | `ftview_audit_entry_t`, `ftview_security_audit_log()` |
| L3.7 | Z-ordered display object tree | `ftview_display_sort_zorder()` |
| L3.8 | Screen navigation graph | `ftview_display_add_nav_link()`, `_navigate()` |
| L3.9 | Time-series circular buffer with dual-index | `ftview_datalog_store_t` + query_range |
| L3.10 | Floating-point deadband compression | `ftview_datalog_swinging_door()` |
| L3.11 | Connection pool with state machines | `ftview_conn_state_t`, `ftview_comm_connect()` |
| L3.12 | Pending request queue with timeout/retry | `ftview_req_t`, `ftview_comm_poll()` |

## L4: Engineering Laws/Standards (Complete ✅)

| ID | Standard / Law | Implementation |
|----|---------------|---------------|
| L4.1 | ISA-101 high-performance HMI colour palette | `ISA101_COLOR_*` macros, greyscale + alarm colours |
| L4.2 | ISA-18.2-2016 alarm management lifecycle | Full state machine in `ftview_alarm_evaluate()` |
| L4.3 | EEMUA 191 alarm rate metrics | `ftview_alarm_rate_per_hour()` |
| L4.4 | ISA-62443-3-3 SR 1.1 (user identification) | `ftview_security_login()` with lockout |
| L4.5 | ISA-62443-3-3 SR 2.1 (authorization) | `ftview_security_check_privilege()` — RBAC |
| L4.6 | 21 CFR Part 11 (electronic signatures) | `ftview_security_esign()` |
| L4.7 | OPC UA Part 2 Security Model | `ftview_comm_security_mode_t` |

## L5: Algorithms/Methods (Complete ✅)

| ID | Algorithm / Method | Complexity | Implementation |
|----|-------------------|-----------|---------------|
| L5.1 | FNV-1a 32-bit hash | O(n) | `ftview_tag_name_hash()` |
| L5.2 | Linear & sqrt engineering unit scaling | O(1) | `ftview_tag_scale_raw_to_eu()` |
| L5.3 | Alarm deadband / hysteresis | O(1) | `ftview_alarm_deadband_check()` |
| L5.4 | On-delay / off-delay alarm timers | O(1) | `ftview_alarm_on_delay_timer()`, `_off_delay` |
| L5.5 | AABB hit-test for touch regions | O(n) | `ftview_display_hit_test()` |
| L5.6 | Bilinear colour interpolation | O(1) | `ftview_color_bilinear()` |
| L5.7 | Swinging-door compression | O(1) | `ftview_datalog_swinging_door()` |
| L5.8 | Piecewise linear interpolation | O(1) | `ftview_datalog_interpolate()` |

## L6: Canonical Problems (Complete ✅)

| ID | Problem | Example File |
|----|--------|-------------|
| L6.1 | Motor start/stop control HMI | ex01_motor_control_hmi.c |
| L6.2 | PID controller faceplate | ex02_pid_faceplate.c |
| L6.3 | Multi-tag alarm management + trending | ex03_alarm_management.c |

## L7: Industrial Applications (Partial+ ✅)

| ID | Application | Implementation |
|----|-----------|---------------|
| L7.1 | PlantPAx HMI object template pattern | `ftview_display_object_t.template_name` |
| L7.2 | OPC-HDA style historical data query | `ftview_datalog_trend_query()` |
| L7.3 | Rockwell RSLinx Classic / FactoryTalk Linx | `ftview_comm_add_topic()`, DRT resolution |
| L7.4 | CIP implicit/explicit messaging | `ftview_comm_connect()` state machine |

## L8: Advanced Topics (Partial ✅)

| ID | Topic | Status |
|----|------|--------|
| L8.1 | High-performance HMI (situation awareness) | ISA-101 colour palette + hierarchy levels |
| L8.2 | Model-based alarm rationalization | ISA-18.2 state machine + rate metrics |

## L9: Research Frontiers (Partial — documented only)

| ID | Topic | Status |
|----|------|--------|
| L9.1 | AR/VR in HMI operations | Documented in course-alignment.md |
| L9.2 | AI-assisted alarm management | Documented |
| L9.3 | Edge-based HMI processing | Documented |
