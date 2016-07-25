import threading
import weakref

class Event(object):
    def __init__(self, name, sync=False):
        self.name = name
        self._syncRoot = threading.Lock()
        self._registrar = {}
        self._sync = sync

    def register(self, func, oneshot=False):
        with self._syncRoot:
            self._registrar[id(func)] = (weakref.ref(func), oneshot)

    def unregister(self, func):
        with self._syncRoot:
            del self._registrar[id(func)]

    def _emit(self, *args, **kwargs):
        self._log.debug("Emitting event")
        with self._syncRoot:
            for funcId, (funcRef, oneshot) in self._registrar.items():
                func = funcRef()
                if func is None or oneshot:
                    del self._registrar[funcId]
                    if func is None:
                        continue
                try:
                    if self._sync:
                        func(*args, **kwargs)
                    else:
                        threading.Thread(target=func, args=args,
                                         kwargs=kwargs).start()
                except:
                    pass

    def emit(self, *args, **kwargs):
        if len(self._registrar) > 0:
            threading.Thread(target=self._emit, args=args,
                             kwargs=kwargs).start()

