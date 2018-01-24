using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Collections.Concurrent;

namespace XXX.Threads
{
    public sealed class CThreadPool : ICThreadPool
    {
        List<CThread> _threads;
        ConcurrentQueue<CThread> _idleThreads;
        int _nowRunningCount = 0,
            //_threadCount = 0,
            _finishedCount = 0,
            _timeoutCount = 0;
        ConcurrentQueue<IDo> _waitingTasks = new ConcurrentQueue<IDo>();
        EventWaitHandle _waitJobHandle;
        volatile int _MaxConcurrentCount;
        const int _maxTryPopFailTimes = 3;

        public CThreadPool(int maxConcurrentCount)
        {
            _threads = new List<CThread>(maxConcurrentCount);
            _MaxConcurrentCount = maxConcurrentCount;
            _idleThreads = new ConcurrentQueue<CThread>();
            _waitJobHandle = new EventWaitHandle(false, EventResetMode.ManualReset);
        }

        public CThreadPool(int maxConcurrentCount, int workItemTimeoutMiliseconds)
            : this(maxConcurrentCount)
        {
            WorkItemTimeoutMiliseconds = workItemTimeoutMiliseconds;
        }

        public static void SetCThreadIdleTimeout(int seconds)
        {
            CThread.ThreadMaxIdleTime = TimeSpan.FromSeconds(seconds);
        }

        public int PoolThreadCount
        {
            get { return _threads.Count; }
        }

        public int IdleThreadCount
        {
            get { return _idleThreads.Count(x => !x.IsShutdown); }
        }

        public int NowRunningWorkCount
        {
            get { return _nowRunningCount; }
        }

        public int NowWaitingWorkCount
        {
            get { return _waitingTasks.Count; }
        }

        public int FinishedWorkCount
        {
            get { return _finishedCount; }
        }

        public int TimeoutWorkCount
        {
            get { return _timeoutCount; }
        }

        public CTask<T> QueueWorkItem<T>(Func<T, int> act)
        {
            var task = new CTask<T>
            {
                StartTime = DateTime.Now,
                Action = act,
                Status = CTaskStatus.NotStart

            };

            _waitingTasks.Enqueue(task);
            NotifyThreadPoolOfPendingWork();
            return task;
        }

        public List<CTask<T>> QueueWorkItem<T>(IEnumerable<Func<T,int>> acts)
        {
            var tasks = new List<CTask<T>>();
            foreach (var act in acts.ToArray())
            {
                var task = new CTask<T>
                {
                    StartTime = DateTime.Now,
                    Action = act,
                    Status = CTaskStatus.NotStart

                };
                _waitingTasks.Enqueue(task);
            }
            
            NotifyThreadPoolOfPendingWork();
            return tasks;
        }


        public List<CTask<T>> QueueWorkItem<T>(params Func<T,int>[] acts)
        {
            var tasks = new List<CTask<T>>();
            foreach (var act in acts)
            {
                var task = new CTask<T>
                {
                    StartTime = DateTime.Now,
                    Action = act,
                    Status = CTaskStatus.NotStart

                };

                _waitingTasks.Enqueue(task);
            }

            NotifyThreadPoolOfPendingWork();
            return tasks;
        }

        private void OnWorkComplete(object cthread)
        {

            var thread = (CThread)cthread;
            Interlocked.Increment(ref _finishedCount);
            IDo task;
            bool hasWorkDo = false;

            if (!thread.IsShutdown)
            {
                while (_waitingTasks.TryDequeue(out task))
                {
                    if (task.IsCanDo(this.WorkItemTimeoutMiliseconds))
                    {
                        thread.AddNewWork(task);
                        hasWorkDo = true;
                        break;
                    }
                    else
                    {
                        Interlocked.Increment(ref _timeoutCount);
                    }
                }
            }

            if (!hasWorkDo)
            {
                Interlocked.Decrement(ref _nowRunningCount);
                _idleThreads.Enqueue(thread);
            }
        }

        private static void ResizeThreadPool(ref CThread[] array, int newSize)
        {
            if (newSize < 0)
                return;

            CThread[] larray = array;
            if (larray == null)
            {
                array = new CThread[newSize];
                return;
            }

            if (larray.Length != newSize)
            {
                CThread[] newArray = new CThread[newSize];

                if (larray.Length < newSize)
                {
                    Array.Copy(larray, 0, newArray, 0, larray.Length > newSize ? newSize : larray.Length);
                }
                else
                {
                    var tmp = larray.Where(x => x != null && !x.IsShutdown).OrderBy(x => !x.IsIdle).ToArray();
                    Array.Copy(tmp, 0, newArray, 0, tmp.Length > newSize ? newSize : tmp.Length);
                    if (tmp.Length > newSize)
                    {
                        foreach (var t in tmp.Skip(newSize).ToArray())
                        {
                            t.Shutdown();
                        }
                    }
                }
                array = newArray;
            }
        }

        private void NotifyThreadPoolOfPendingWork()
        {
            if (_nowRunningCount < this.MaxConcurrentCount)
            {
                var canRunCount = this.MaxConcurrentCount - _nowRunningCount;
                var failPopTimes = 0;
                while (_waitingTasks.Count > 0 && _idleThreads.Count > 0 && failPopTimes < _maxTryPopFailTimes)
                {
                    CThread worker;
                    if (!_idleThreads.TryDequeue(out worker))
                    {
                        failPopTimes++;
                        continue;
                    }

                    if (worker.IsShutdown)
                    {
                        _threads.Remove(worker);
                        //Interlocked.Decrement(ref _threadCount);
                        continue;
                    }

                    IDo task;
                    if (_waitingTasks.TryDequeue(out task))
                    {
                        if (task.IsCanDo(this.WorkItemTimeoutMiliseconds))
                        {
                            worker.DoWork(task);
                            Interlocked.Increment(ref _nowRunningCount);
                        }
                        else
                        {
                            Interlocked.Increment(ref _timeoutCount);
                        }
                    }
                }


                while (_waitingTasks.Count > 0 && _nowRunningCount < this.MaxConcurrentCount)
                {

                    IDo task;
                    if (_waitingTasks.TryDequeue(out task))
                    {
                        if (task.IsCanDo(this.WorkItemTimeoutMiliseconds))
                        {
                            CThread thread = new CThread(OnWorkComplete);
                            thread.DoWork(task);
                            Interlocked.Increment(ref _nowRunningCount);
                            //Interlocked.Increment(ref _threadCount);
                            //_threads[_threadCount - 1] = thread;
                            _threads.Add(thread);
                        }
                    }
                }
            }
        }


        public int MaxConcurrentCount
        {
            set
            {
                if (_MaxConcurrentCount != value)
                {
                    var moreWorker = _MaxConcurrentCount < value;
                    _MaxConcurrentCount = value;
                    //ResizeThreadPool(ref _threads, _MaxConcurrentCount);
                    //Interlocked.Exchange(ref _threadCount, _threads.Count(x => x != null));
                    ResizeThreadPool(_threads,_MaxConcurrentCount);
                    if (moreWorker)
                        NotifyThreadPoolOfPendingWork();
                }
            }
            get
            {
                return _MaxConcurrentCount;
            }
        }

        private void ResizeThreadPool(List<CThread> _threads, int _MaxConcurrentCount)
        {
            _threads.RemoveAll(x => x.IsShutdown);

            var diff = _threads.Count - _MaxConcurrentCount;
            if (diff > 0)
            {
                var idleThreads = _threads.Where(x => x.IsIdle && x.ThreadID > 0).ToArray();
                var idleDiff = idleThreads.Count() - diff;

                if (idleDiff >= 0)
                {
                    foreach (var item in idleThreads)
                    {
                        item.Shutdown();
                        _threads.Remove(item);
                    }

                    _idleThreads = new ConcurrentQueue<CThread>();
                    if (idleDiff > 0)
                    {
                        var needShutdownThread = _threads.Take(idleDiff).ToArray();
                        var ids = new int[needShutdownThread.Length];
                        for (int i = 0; i < needShutdownThread.Length; i++)
                        {
                            needShutdownThread[i].Shutdown();
                            ids[0] = needShutdownThread[i].ThreadID;
                        }
                        _threads.RemoveAll(x => ids.Contains(x.ThreadID));
                    }
                }
                else
                {
                    for (int i = 0; i < -idleDiff && _threads.Count > 0; i++)
                    {
                        CThread thread = _threads[0];
                        thread.Shutdown();
                        _threads.Remove(thread);
                    }
                }
            }
        }

        public int WorkItemTimeoutMiliseconds
        {
            set;
            get;
        }
    }
}
