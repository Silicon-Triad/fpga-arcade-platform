# `sw/hal/`

**Owner:** Mujtaba  
**Issues:** #4, #34–#36, #43, #44

Hardware Access Layer: bridge mapping, shadow tilemap, present(), audio registers, codec I2C. `register_map.hpp` is FROZEN and only this folder may include it (QA-01). SRS: SR-HAL, §3.1.4.

Rules: see [`AGENTS.md`](/AGENTS.md).
