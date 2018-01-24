using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace XXX.Threads
{
    public enum CTaskStatus
    {
        NotStart, Failed, Canceld, Completed, Running, Timeout
    }

    public interface IDo
    {
        void Do();

        bool IsCanDo(int timeout);

        int WorkerThreadID { get; }
    }

    public sealed class CTask<T>:IDo
    {
        private int _result;

        internal DateTime StartTime { set; get; }

        internal DateTime EndTime { set; get; }

        internal DateTime RealStartTime { set; get; }

        public Func<T,int> Action { set; get; }

        public T Param { set; get; }

        public Exception Exception { get; internal set; }

        public int WorkerThreadID { internal set; get; }

        int _realExeMilliseconds = 0;
        int _exeMilliseconds = 0;

        public int Result
        {
            get
            {
                if (IsCompleted || IsCanceled){
                    return _result;
                }
                else
                {
                    this.Wait();
                    return _result;
                }
            }
        }

        public bool Wait(int waitMilliseconds=-1)
        {
            int alreadyWait = 0;
            int waitUnit = 1000;

            while (alreadyWait<waitMilliseconds||waitMilliseconds<0)
            {
               
                if (this.IsCompleted || this.IsCanceled)
                {
                    return true;
                }
                Thread.Sleep(waitUnit);
                alreadyWait += waitUnit;
                
            }
            return false;
        }

        public void Do()
        {
            WorkerThreadID = Thread.CurrentThread.ManagedThreadId;
            this.RealStartTime = DateTime.Now;
            Status = CTaskStatus.Running;

            try
            {
                _result = Action(Param);
                Status = CTaskStatus.Completed;
                
            }
            catch (Exception ex)
            {
                this.IsCompleted = true;
                this.Status = CTaskStatus.Failed;
                this.IsFaulted = true;
                this.Exception = ex;

            }

            IsCompleted = true;
            EndTime = DateTime.Now;
        }

        public int ExeuteMilliseconds
        {
            get
            {
                if (IsCompleted || IsCanceled)
                {
                    if (0 == _exeMilliseconds)
                        _exeMilliseconds = (int)(EndTime - StartTime).TotalMilliseconds;
                    return _exeMilliseconds;
                }
                else
                {
                    return (int)(DateTime.Now - StartTime).TotalMilliseconds;
                }
            }
        }
        public int RealExecuteMilliseconds
        {
            get
            {
                if (IsCompleted || IsCanceled)
                {
                    if (0 == _realExeMilliseconds)
                        _realExeMilliseconds = (int)(EndTime - RealStartTime).TotalMilliseconds;
                    return _realExeMilliseconds;
                }
                else
                {
                    return (int)(DateTime.Now - StartTime).TotalMilliseconds;
                }
            }
        }

        public bool IsCompleted {private set; get; }

        public bool IsCanceled { private set; get; }

        public bool IsFaulted { private set; get; }

        public CTaskStatus Status {internal set; get; }

        public bool IsCanDo(int timeout)
        {

            if (IsCompleted || IsCanceled)
                return false;
            else if (timeout > 0)
            {
                if( timeout > ExeuteMilliseconds)
                {
                    return true;
                }
                else
                {
                    this.Status = CTaskStatus.Timeout;
                    this.IsCanceled = true;
                    this.EndTime = DateTime.Now;
                    return false;
                }
            }
            else
            {
                return true;
            }
        }
    }
}
