from __future__ import annotations

from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity import Entity

from .const import DEVICE_ID, DEVICE_NAME, DOMAIN, MANUFACTURER, MODEL
from .hub import BoilerBridgeHub

class BoilerBridgeEntity(Entity):
    _attr_has_entity_name = True
    _attr_should_poll = False

    def __init__(self, hub: BoilerBridgeHub) -> None:
        self._hub = hub

    @property
    def device_info(self) -> DeviceInfo:
        return DeviceInfo(
            identifiers={(DOMAIN, DEVICE_ID)},
            name=DEVICE_NAME,
            manufacturer=MANUFACTURER,
            model=MODEL,
        )
