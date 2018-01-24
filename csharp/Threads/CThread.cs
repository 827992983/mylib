using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace XXX.Threads
{
    internal sealed class CThread
    {
        private BlockingQueue<IDo> _tasks;
        private Thread _thread=null;
        private EventWaitHandle _newJobWaitHandle;
        private volatile bool _isIdle = true;
        private WaitCallback _completeCallback;
        private bool _isShutdown = false;
        private int _threadID = -1;
        public static TimeSpan ThreadMaxIdleTime = TimeSpan.FromMinutes(5);

        public CThread(WaitCallback completeCallback)
        {
            _newJobWaitHandle = new EventWaitHandle(false, EventResetMode.ManualReset);
            _completeCallback = completeCallback;
            _tasks = new BlockingQueue<IDo>();
        }

        public void AddNewWork(IDo task)
        {
            _tasks.Enqueue(task);
        }

        public int ThreadID
        {
            get
            {
                return _threadID;
            }
        }

        public void DoWork(IDo task)
        {
            if (_isIdle)
            {
                _tasks.Enqueue(task);
                //Thread.Sleep(1);      
      
                if (null == _thread)
                {
                    _thread = new Thread(DoTask);
                    _thread.IsBackground = true;
                    _threadID = _thread.ManagedThreadId;
                    _thread.Start();
                }
                else
                {
                    _newJobWaitHandle.Set();
                }
            }
            else
            {
                throw (new ThreadStateException());
            }
        }

        void DoTask()
        {
            while (true && !_isShutdown)
            {
                _isIdle = false;

                while (_tasks.Size > 0)
                {
                    lock (_tasks)
                    {
                        var task = _tasks.Dequeue();
                        task.Do();
                    }

                    _isIdle = true;
                    try
                    {
                        _completeCallback(this);
                    }
                    catch (Exception ex)
                    {
                        throw ex;
                    }
                }

                if (!_isShutdown)
                {
                    _newJobWaitHandle.Reset();
                    if (!_newJobWaitHandle.WaitOne(ThreadMaxIdleTime))
                    {
                        _isShutdown = true;
                        _isIdle = false;
                        _thread = null;
                    }
                }
            }
        }

        public bool IsIdle
        {
            get { return _isIdle; }
        }

        public bool IsShutdown
        {
            get
            {
                return _isShutdown;
            }
        }

        public void Shutdown()
        {
            _isShutdown = true;
            _isIdle = false;
            _thread = null;
            _newJobWaitHandle.Set();
        }
    } 
}
