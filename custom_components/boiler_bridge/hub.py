from __future__ import annotations

import asyncio
import logging
from collections.abc import Callable

from homeassistant.core import HomeAssistant, State, callback
from homeassistant.helpers.event import async_track_state_change_event

from .const import (
    SOURCE_MODE_BUTTON, SOURCE_MODE_HOME_BUTTON, SOURCE_MODE_RAW, SOURCE_PAGE,
    SOURCE_START_BUTTON, SOURCE_SUPPORT_BUTTON,
)

_LOGGER = logging.getLogger(__name__)

def support_is_on(raw: str | None) -> bool | None:
    if not raw or raw in ("unknown", "unavailable"):
        return None
    parts = [p.strip() for p in raw.split(",")]
    if len(parts) < 3:
        return None
    if parts[2] == "2":
        return True
    if parts[2] == "1":
        return False
    return None

def run_is_on(raw: str | None) -> bool | None:
    if not raw or raw in ("unknown", "unavailable"):
        return None
    parts = [p.strip() for p in raw.split(",")]
    if len(parts) < 2:
        return None
    if parts[1] == "2":
        return True
    if parts[1] == "1":
        return False
    return None

def _state_page(state: State) -> int | None:
    try:
        return int(float(state.state))
    except (TypeError, ValueError):
        return None

class BoilerBridgeHub:
    def __init__(self, hass: HomeAssistant) -> None:
        self.hass = hass
        self.lock = asyncio.Lock()
        self.last_result = "idle"
        self._listeners: set[Callable[[], None]] = set()

    def add_listener(self, listener: Callable[[], None]) -> Callable[[], None]:
        self._listeners.add(listener)
        def _remove() -> None:
            self._listeners.discard(listener)
        return _remove

    @callback
    def _set_result(self, value: str) -> None:
        self.last_result = value
        for listener in tuple(self._listeners):
            listener()

    @property
    def page(self) -> int | None:
        state = self.hass.states.get(SOURCE_PAGE)
        if state is None or state.state in ("unknown", "unavailable"):
            return None
        return _state_page(state)

    @property
    def mode_raw(self) -> str | None:
        state = self.hass.states.get(SOURCE_MODE_RAW)
        return None if state is None else state.state

    @property
    def support_on(self) -> bool | None:
        return support_is_on(self.mode_raw)

    @property
    def run_on(self) -> bool | None:
        return run_is_on(self.mode_raw)

    def source_available(self, entity_id: str) -> bool:
        state = self.hass.states.get(entity_id)
        return state is not None and state.state not in ("unknown", "unavailable")

    async def _press_and_wait(
        self, source_button: str, watch_entity: str, predicate: Callable[[State], bool],
        description: str, timeout: float = 5.0,
    ) -> bool:
        async with self.lock:
            if not self.source_available(source_button):
                self._set_result(f"unavailable:{description}")
                return False

            current = self.hass.states.get(watch_entity)
            if current is not None and predicate(current):
                self._set_result(f"ok:{description}:already")
                return True

            future = self.hass.loop.create_future()

            @callback
            def _state_changed(event):
                new_state = event.data.get("new_state")
                if new_state is not None and predicate(new_state) and not future.done():
                    future.set_result(True)

            unsubscribe = async_track_state_change_event(self.hass, [watch_entity], _state_changed)
            try:
                self._set_result(f"sending:{description}")
                await self.hass.services.async_call(
                    "button", "press", target={"entity_id": source_button}, blocking=True,
                )
                current = self.hass.states.get(watch_entity)
                if current is not None and predicate(current):
                    self._set_result(f"ok:{description}")
                    return True
                try:
                    await asyncio.wait_for(future, timeout=timeout)
                except TimeoutError:
                    await asyncio.sleep(2.0)
                    current = self.hass.states.get(watch_entity)
                    if current is not None and predicate(current):
                        self._set_result(f"ok:{description}:late")
                        return True
                    self._set_result(f"timeout:{description}")
                    _LOGGER.warning("Boiler command timed out: %s", description)
                    return False
                self._set_result(f"ok:{description}")
                return True
            except Exception as exc:
                self._set_result(f"error:{description}:{type(exc).__name__}")
                raise
            finally:
                unsubscribe()

    async def set_support(self, turn_on: bool) -> bool:
        current = self.support_on
        if current is None:
            self._set_result("unavailable:support_state")
            return False
        if current is turn_on:
            self._set_result("ok:support_on:already" if turn_on else "ok:support_off:already")
            return True
        return await self._press_and_wait(
            SOURCE_SUPPORT_BUTTON, SOURCE_MODE_RAW,
            lambda s: support_is_on(s.state) is turn_on,
            "support_on" if turn_on else "support_off",
        )

    async def set_run(self, turn_on: bool) -> bool:
        current = self.run_on
        description = "run_on" if turn_on else "run_off"
        if current is None:
            self._set_result("unavailable:run_state")
            return False
        if current is turn_on:
            self._set_result(f"ok:{description}:already")
            return True

        async with self.lock:
            if not self.source_available(SOURCE_START_BUTTON):
                self._set_result(f"unavailable:{description}")
                return False
            try:
                self._set_result(f"sending:{description}")
                await self.hass.services.async_call(
                    "button", "press", target={"entity_id": SOURCE_START_BUTTON}, blocking=True,
                )

                deadline = self.hass.loop.time() + 10.0
                stable_since: float | None = None
                while self.hass.loop.time() < deadline:
                    now = self.hass.loop.time()
                    if self.run_on is turn_on:
                        if stable_since is None:
                            stable_since = now
                        elif now - stable_since >= 3.0:
                            self._set_result(f"ok:{description}:stable")
                            return True
                    else:
                        stable_since = None
                    await asyncio.sleep(0.25)

                self._set_result(f"timeout:{description}:not_stable")
                _LOGGER.warning("Boiler run state did not stabilize: %s", description)
                return False
            except Exception as exc:
                self._set_result(f"error:{description}:{type(exc).__name__}")
                raise

    async def open_mode_screen(self) -> bool:
        return await self._press_and_wait(
            SOURCE_MODE_BUTTON, SOURCE_PAGE, lambda s: _state_page(s) == 2, "open_mode_screen"
        )

    async def go_home_from_mode(self) -> bool:
        return await self._press_and_wait(
            SOURCE_MODE_HOME_BUTTON, SOURCE_PAGE, lambda s: _state_page(s) == 1, "home_from_mode"
        )
