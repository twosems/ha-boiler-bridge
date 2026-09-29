from __future__ import annotations

from homeassistant.components.sensor import SensorEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddConfigEntryEntitiesCallback
from homeassistant.helpers.event import async_track_state_change_event

from .const import SOURCE_MODE_RAW, SOURCE_PAGE
from .entity import BoilerBridgeEntity
from .hub import BoilerBridgeHub

async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddConfigEntryEntitiesCallback) -> None:
    hub: BoilerBridgeHub = entry.runtime_data
    async_add_entities([
        BoilerPageSensor(hub),
        BoilerModeRawSensor(hub),
        BoilerStartRawSensor(hub),
        BoilerStartStateSensor(hub),
        BoilerCommandStatusSensor(hub),
    ])

class _MirrorSensor(BoilerBridgeEntity, SensorEntity):
    _source_entity: str

    async def async_added_to_hass(self) -> None:
        await super().async_added_to_hass()
        @callback
        def _changed(event):
            self.async_write_ha_state()
        self.async_on_remove(async_track_state_change_event(self.hass, [self._source_entity], _changed))

    @property
    def available(self) -> bool:
        return self._hub.source_available(self._source_entity)

class BoilerPageSensor(_MirrorSensor):
    _attr_name = "Текущая страница"
    _attr_unique_id = "boiler_bridge_current_page"
    _attr_icon = "mdi:page-layout-body"
    _source_entity = SOURCE_PAGE

    @property
    def native_value(self):
        state = self.hass.states.get(self._source_entity)
        if state is None:
            return None
        try:
            return int(float(state.state))
        except (TypeError, ValueError):
            return None

class BoilerModeRawSensor(_MirrorSensor):
    _attr_name = "MODE_RAW"
    _attr_unique_id = "boiler_bridge_mode_raw"
    _attr_icon = "mdi:code-array"
    _source_entity = SOURCE_MODE_RAW

    @property
    def native_value(self):
        state = self.hass.states.get(self._source_entity)
        return None if state is None else state.state

    @property
    def extra_state_attributes(self):
        parts = _mode_raw_parts(self.hass)
        if parts is None:
            return {"start_raw": None, "start_state": None}
        start_raw = parts[1]
        if start_raw == 1:
            start_state = "Пуск"
        elif start_raw == 2:
            start_state = "Работа"
        else:
            start_state = f"Неизвестно ({start_raw})"
        return {
            "start_raw": start_raw,
            "start_state": start_state,
            "start_mapping": "1=Пуск (красная); 2=Работа (зелёная)",
        }


def _mode_raw_parts(hass: HomeAssistant) -> list[int] | None:
    state = hass.states.get(SOURCE_MODE_RAW)
    if state is None:
        return None
    try:
        parts = [int(part.strip()) for part in state.state.split(",")]
    except (TypeError, ValueError):
        return None
    return parts if len(parts) >= 4 else None


class BoilerStartRawSensor(_MirrorSensor):
    _attr_name = "START_RAW"
    _attr_unique_id = "boiler_bridge_start_raw"
    _attr_icon = "mdi:numeric"
    _source_entity = SOURCE_MODE_RAW

    @property
    def native_value(self):
        parts = _mode_raw_parts(self.hass)
        return None if parts is None else parts[1]


class BoilerStartStateSensor(_MirrorSensor):
    _attr_name = "Пуск/Работа"
    _attr_unique_id = "boiler_bridge_start_state"
    _attr_icon = "mdi:engine"
    _source_entity = SOURCE_MODE_RAW

    @property
    def native_value(self):
        parts = _mode_raw_parts(self.hass)
        if parts is None:
            return None
        start_raw = parts[1]
        if start_raw == 1:
            return "Пуск"
        if start_raw == 2:
            return "Работа"
        return f"Неизвестно ({start_raw})"

    @property
    def extra_state_attributes(self):
        parts = _mode_raw_parts(self.hass)
        return {
            "start_raw": None if parts is None else parts[1],
            "mode_raw": None if parts is None else ",".join(str(v) for v in parts),
            "confirmed_mapping": "1=Пуск (красная); 2=Работа (зелёная)",
        }


class BoilerCommandStatusSensor(BoilerBridgeEntity, SensorEntity):
    _attr_name = "Последняя команда"
    _attr_unique_id = "boiler_bridge_last_command"
    _attr_icon = "mdi:message-processing-outline"

    async def async_added_to_hass(self) -> None:
        await super().async_added_to_hass()
        @callback
        def _changed():
            self.async_write_ha_state()
        self.async_on_remove(self._hub.add_listener(_changed))

    @property
    def native_value(self):
        return self._hub.last_result
