from __future__ import annotations

from functools import update_wrapper, wraps
from typing import TYPE_CHECKING, Any, Callable, TypeVar

if TYPE_CHECKING:
    T = TypeVar("T", bound=Callable[..., Any])

    def cached_method(func: T) -> T: ...

else:

    class cached_method:
        def __init__(self, method):
            self._method = method
            update_wrapper(self, method)

        def __get__(self, instance, objtype=None):
            if instance is None:
                return self

            cache = {}

            @wraps(self._method)
            def lookup(*args, **kwargs):
                key = (args, tuple(kwargs.items()))
                try:
                    return cache[key]
                except KeyError:
                    res = self._method(instance, *args, **kwargs)
                    cache[key] = res
                    return res

            lookup.cache = cache

            setattr(instance, self._method.__name__, lookup)
            return lookup
