from homeassistant.const import Platform

DOMAIN = "boiler_bridge"
PLATFORMS: tuple[Platform, ...] = (Platform.SENSOR, Platform.SWITCH, Platform.BUTTON)

SOURCE_PAGE = "sensor.kotel_uart_proxy_tekushchaia_stranitsa_page"
SOURCE_MODE_RAW = "sensor.timiriazevo_kotel_uart_proxy_rezhim_raw_mode_raw"
SOURCE_SUPPORT_BUTTON = "button.timiriazevo_kotel_uart_proxy_glavnaia_podderzhka_supportbtn"
SOURCE_START_BUTTON = "button.timiriazevo_kotel_uart_proxy_glavnaia_pusk_p1start"
SOURCE_MODE_BUTTON = "button.timiriazevo_kotel_uart_proxy_glavnaia_rezhim_modebtn"
SOURCE_MODE_HOME_BUTTON = "button.timiriazevo_kotel_uart_proxy_rezhim_na_glavnuiu_modehome"

DEVICE_ID = "boiler_uart_bridge"
DEVICE_NAME = "Котёл — UART Bridge"
MANUFACTURER = "Custom UART Bridge"
MODEL = "ESP32 / Nextion proxy"
