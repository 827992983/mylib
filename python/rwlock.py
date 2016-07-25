import threading
import Queue

class RWLock(object):
    """
    A simple ReadWriteLock implementation.

    The lock must be released by the thread that acquired it.  Once a thread
    has acquired a lock, the same thread may acquire it again without blocking;
    the thread must release it once for each time it has acquired it. Note that
    lock promotion (acquiring an exclusive lock under a shared lock is
    forbidden and will raise an exception.

    The lock puts all requests in a queue. The request is granted when The
    previous one is released.

    Each request is represented by a :class:`tht` object. When the
    Event is set the request is granted. This enables multiple callers to wait
    for a request thus implementing a shared lock.
    """
    class _contextLock(object):
        def __init__(self, owner, exclusive):
            self._owner = owner
            self._exclusive = exclusive

        def __enter__(self):
            self._owner.acquire(self._exclusive)

        def __exit__(self, exc_type, exc_value, traceback):
            self._owner.release()

    def __init__(self):
        self._syncRoot = threading.Lock()
        self._queue = Queue.Queue()
        self._currentSharedLock = None
        self._currentState = None
        self._holdingThreads = {}

        self.shared = self._contextLock(self, False)
        self.exclusive = self._contextLock(self, True)

    def acquireRead(self):
        return self.acquire(False)

    def acquireWrite(self):
        return self.acquire(True)

    def acquire(self, exclusive):
        currentEvent = None
        currentThread = threading.currentThread()

        # Handle reacquiring lock in the same thread
        if currentThread in self._holdingThreads:
            if self._currentState is False and exclusive:
                raise RuntimeError("Lock promotion is forbidden.")

            self._holdingThreads[currentThread] += 1
            return

        with self._syncRoot:
            # Handle regular acquisition
            if exclusive:
                currentEvent = threading.Event()
                self._currentSharedLock = None
            else:
                if self._currentSharedLock is None:
                    self._currentSharedLock = threading.Event()

                currentEvent = self._currentSharedLock

            try:
                self._queue.put_nowait((currentEvent, exclusive))
            except Queue.Full:
                raise RuntimeError("There are too many objects waiting for "
                                   "this lock")

            if self._queue.unfinished_tasks == 1:
                # Bootstrap the process if needed. A lock is released the when
                # the next request is granted. When there is no one to grant
                # the request you have to grant it yourself.
                event, self._currentState = self._queue.get_nowait()
                event.set()

        currentEvent.wait()

        self._holdingThreads[currentThread] = 0

    def release(self):
        currentThread = threading.currentThread()

        if currentThread not in self._holdingThreads:
            raise RuntimeError("Releasing an lock without acquiring it first")

        # If in nested lock don't really release
        if self._holdingThreads[currentThread] > 0:
            self._holdingThreads[currentThread] -= 1
            return

        del self._holdingThreads[currentThread]

        with self._syncRoot:
            self._queue.task_done()

            if self._queue.empty():
                self._currentState = None
                return

            nextRequest, self._currentState = self._queue.get_nowait()

        nextRequest.set()

