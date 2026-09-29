from __future__ import annotations

from homeassistant.components.button import ButtonEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers.entity_platform import AddConfigEntryEntitiesCallback

from .const import SOURCE_MODE_BUTTON, SOURCE_MODE_HOME_BUTTON
from .entity import BoilerBridgeEntity
from .hub import BoilerBridgeHub

async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddConfigEntryEntitiesCallback) -> None:
    hub: BoilerBridgeHub = entry.runtime_data
    async_add_entities([BoilerModeButton(hub), BoilerHomeButton(hub)])

class _TrackedSourceButton(BoilerBridgeEntity, ButtonEntity):
    _source_entity: str

    @property
    def available(self) -> bool:
        return self._hub.source_available(self._source_entity)

class BoilerModeButton(_TrackedSourceButton):
    _attr_name = "Открыть режим работы"
    _attr_unique_id = "boiler_bridge_open_mode"
    _attr_icon = "mdi:cog"
    _source_entity = SOURCE_MODE_BUTTON

    async def async_press(self) -> None:
        if not await self._hub.open_mode_screen():
            raise HomeAssistantError("Котёл не подтвердил переход на экран режима")

class BoilerHomeButton(_TrackedSourceButton):
    _attr_name = "На главную из режима"
    _attr_unique_id = "boiler_bridge_home_from_mode"
    _attr_icon = "mdi:home"
    _source_entity = SOURCE_MODE_HOME_BUTTON

    async def async_press(self) -> None:
        if not await self._hub.go_home_from_mode():
            raise HomeAssistantError("Котёл не подтвердил возврат на главный экран")
