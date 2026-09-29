from __future__ import annotations

import voluptuous as vol
from homeassistant import config_entries

from .const import DOMAIN

class BoilerBridgeConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    VERSION = 1

    async def _create(self):
        await self.async_set_unique_id("boiler_uart_bridge_v1")
        self._abort_if_unique_id_configured()
        return self.async_create_entry(title="Котёл — UART Bridge", data={})

    async def async_step_user(self, user_input=None):
        await self.async_set_unique_id("boiler_uart_bridge_v1")
        self._abort_if_unique_id_configured()
        if user_input is not None:
            return self.async_create_entry(title="Котёл — UART Bridge", data={})
        return self.async_show_form(step_id="user", data_schema=vol.Schema({}))

    async def async_step_import(self, user_input=None):
        return await self._create()
