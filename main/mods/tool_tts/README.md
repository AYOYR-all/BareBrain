# TW-TTS Tool

Registers `tts_say` and `tts_control` for speech output through the local TW-TTS UART module.

Permissions: `uart`.

Configuration: UART and pin settings come from the voice/TTS firmware configuration.

Known limits: the tools degrade with a readable error when the module is unavailable.
