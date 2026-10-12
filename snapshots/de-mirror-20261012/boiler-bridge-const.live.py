from homeassistant.const import Platform

DOMAIN = "boiler_bridge"
CONF_SOURCE_DEVICE_ID = "source_device_id"
CARD_API_VERSION = 2

PLATFORMS: tuple[Platform, ...] = (Platform.SENSOR, Platform.SWITCH, Platform.BUTTON)

DEVICE_ID = "boiler_uart_bridge"
DEVICE_NAME = "Котёл — UART Bridge"
MANUFACTURER = "Custom UART Bridge"
MODEL = "ESP32 / Nextion proxy"

# Stable firmware tokens. Entity IDs may be renamed freely; Boiler Bridge resolves
# the current IDs from Home Assistant's entity registry by these original tokens.
SOURCE_ROLE_CODES: dict[str, str] = {
    # Data
    "page": "PAGE",
    "modeRaw": "MODE_RAW",
    "screen": "SCREEN",
    "bt": "BTEMP",
    "dt": "DTEMP",
    "bs": "BSP",
    "ds": "DSP",
    "autofan": "AUTOFAN",
    "autofanMin": "AUTOFAN_MIN",
    "autofanMax": "AUTOFAN_MAX",
    "autofeed": "AUTOFEED",
    "autofeedMin": "AUTOFEED_MIN",
    "autofeedMax": "AUTOFEED_MAX",
    "autopause": "AUTOPAUSE",
    "supblow": "SUPBLOW",
    "supfeed": "SUPFEED",
    "suppause": "SUPPAUSE",
    "ignition": "IGNITIONFAN",
    "modeLabel": "MODE_LABEL",
    "dhwTopText": "P6TOP_TEXT",
    "roomThermostatText": "P6BOTTOM_TEXT",
    "ignitionFanText": "P11FAN_TEXT",
    "ignitionFanState": "P11FAN_STATE",
    "feedText": "P12FEED_TEXT",
    "reverseText": "P12REVERSE_TEXT",
    "screensaverText": "P20TOGGLE_TEXT",
    "passwordKeymap": "P36_KEYMAP",
    "feedState": "P12FEED_STATE",
    "reverseState": "P12REVERSE_STATE",
    "screensaverState": "P20TOGGLE_STATE",
    "motor": "MOTAMP",
    "reverse": "REVAMP",
    "kp": "KP",
    "ki": "KI",
    "kd": "KD",
    # Internal bridge controls
    "_startButton": "P1START",
    "_supportButton": "SUPPORTBTN",
    "_modeButton": "MODEBTN",
    "_modeHomeRaw": "MODEHOME",
    "_refresh": "REFRESH",
    # Full control card
    "boilerSet": "BSPBTN",
    "dhwSet": "DSPBTN",
    "p2auto": "P2AUTO",
    "p2ignition": "P2IGNITION",
    "p2support": "P2SUPPORT",
    "p2settings": "MODESET",
    "p3minus": "P3MINUS", "p3plus": "P3PLUS", "p3confirm": "P3CONFIRM",
    "p3back": "P3BACK", "p3home": "P3HOME", "p3next": "P3NEXT",
    "p4minus": "P4MINUS", "p4plus": "P4PLUS", "p4confirm": "P4CONFIRM",
    "p4back": "P4BACK", "p4home": "P4HOME", "p4next": "P4NEXT",
    "p5minus": "P5MINUS", "p5plus": "P5PLUS", "p5confirm": "P5CONFIRM",
    "p5back": "P5BACK", "p5home": "P5HOME", "p5next": "P5NEXT",
    "p6top": "P6TOP", "p6bottom": "P6BOTTOM", "p6back": "P6BACK",
    "p6home": "P6HOME", "p6next": "P6NEXT",
    "p7minus": "P7MINUS", "p7plus": "P7PLUS", "p7confirm": "P7CONFIRM",
    "p7back": "P7BACK", "p7home": "P7HOME", "p7next": "P7NEXT",
    "p8minus": "P8MINUS", "p8plus": "P8PLUS", "p8confirm": "P8CONFIRM",
    "p8back": "P8BACK", "p8home": "P8HOME", "p8next": "P8NEXT",
    "p9minus": "P9MINUS", "p9plus": "P9PLUS", "p9confirm": "P9CONFIRM",
    "p9back": "P9BACK", "p9home": "P9HOME", "p9next": "P9NEXT",
    "p10bminus": "P10BMINUS", "p10bplus": "P10BPLUS",
    "p10dminus": "P10DMINUS", "p10dplus": "P10DPLUS",
    "p10back": "P10BACK", "p10home": "P10HOME",
    "p11minus": "P11MINUS", "p11plus": "P11PLUS", "p11fan": "P11FAN",
    "p11home": "P11HOME", "p11next": "P11NEXT",
    "p12feed": "P12FEED", "p12reverse": "P12REVERSE", "p12back": "P12BACK",
    "p12home": "P12HOME", "p12next": "P12NEXT",
    "p16minus": "P16MINUS", "p16plus": "P16PLUS", "p16back": "P16BACK",
    "p16home": "P16HOME", "p16next": "P16NEXT",
    "p17minus": "P17MINUS", "p17plus": "P17PLUS", "p17back": "P17BACK",
    "p17home": "P17HOME", "p17next": "P17NEXT",
    "p18minus": "P18MINUS", "p18plus": "P18PLUS", "p18back": "P18BACK",
    "p18home": "P18HOME", "p18next": "P18NEXT",
    "p19minus": "P19MINUS", "p19plus": "P19PLUS", "p19back": "P19BACK",
    "p19home": "P19HOME", "p19next": "P19NEXT",
    "p20toggle": "P20TOGGLE", "p20password": "P20PASSWORD", "p20back": "P20BACK",
    "p20home": "P20HOME", "p20next": "P20NEXT",
    "p23reset": "P23RESET", "p23back": "P23BACK", "p23home": "P23HOME",
    "p33home": "P33HOME",
    "p36id4": "P36ID4", "p36id5": "P36ID5", "p36id6": "P36ID6",
    "p36id7": "P36ID7", "p36id8": "P36ID8", "p36id9": "P36ID9",
    "p36id10": "P36ID10", "p36id11": "P36ID11", "p36id12": "P36ID12",
    "p36id13": "P36ID13", "p36id14": "P36ID14", "p36id15": "P36ID15",
    "p36id22": "P36ID22", "p36id24": "P36ID24", "p36id25": "P36ID25",
}

REQUIRED_CODES = {"PAGE", "MODE_RAW", "P1START", "MODEBTN", "SUPPORTBTN"}
