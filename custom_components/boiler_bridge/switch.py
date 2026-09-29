from __future__ import annotations

from homeassistant.components.switch import SwitchEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers.entity_platform import AddConfigEntryEntitiesCallback
from homeassistant.helpers.event import async_track_state_change_event

from .const import SOURCE_MODE_RAW, SOURCE_START_BUTTON, SOURCE_SUPPORT_BUTTON
from .entity import BoilerBridgeEntity
from .hub import BoilerBridgeHub

async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddConfigEntryEntitiesCallback) -> None:
    hub: BoilerBridgeHub = entry.runtime_data
    async_add_entities([BoilerSupportSwitch(hub), BoilerRunSwitch(hub)])

class BoilerSupportSwitch(BoilerBridgeEntity, SwitchEntity):
    _attr_name = "Поддержка"
    _attr_unique_id = "boiler_bridge_support"
    _attr_icon = "mdi:fire-circle"

    async def async_added_to_hass(self) -> None:
        await super().async_added_to_hass()
        @callback
        def _changed(event):
            self.async_write_ha_state()
        self.async_on_remove(async_track_state_change_event(self.hass, [SOURCE_MODE_RAW, SOURCE_SUPPORT_BUTTON], _changed))

    @property
    def is_on(self) -> bool | None:
        return self._hub.support_on

    @property
    def available(self) -> bool:
        return self._hub.support_on is not None and self._hub.source_available(SOURCE_SUPPORT_BUTTON)

    async def async_turn_on(self, **kwargs) -> None:
        if not await self._hub.set_support(True):
            raise HomeAssistantError("Котёл не подтвердил включение Поддержки")

    async def async_turn_off(self, **kwargs) -> None:
        if not await self._hub.set_support(False):
            raise HomeAssistantError("Котёл не подтвердил выключение Поддержки")


class BoilerRunSwitch(BoilerBridgeEntity, SwitchEntity):
    _attr_name = "Работа"
    _attr_unique_id = "boiler_bridge_run"
    _attr_icon = "mdi:engine"

    async def async_added_to_hass(self) -> None:
        await super().async_added_to_hass()
        @callback
        def _changed(event):
            self.async_write_ha_state()
        self.async_on_remove(async_track_state_change_event(self.hass, [SOURCE_MODE_RAW, SOURCE_START_BUTTON], _changed))

    @property
    def is_on(self) -> bool | None:
        return self._hub.run_on

    @property
    def available(self) -> bool:
        return self._hub.run_on is not None and self._hub.source_available(SOURCE_START_BUTTON)

    async def async_turn_on(self, **kwargs) -> None:
        if not await self._hub.set_run(True):
            raise HomeAssistantError("Котёл не подтвердил устойчивый переход в режим Работа")

    async def async_turn_off(self, **kwargs) -> None:
        if not await self._hub.set_run(False):
            raise HomeAssistantError("Котёл не подтвердил устойчивый переход в режим Пуск")
