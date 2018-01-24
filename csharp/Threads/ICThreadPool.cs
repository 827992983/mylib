using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace XXX.Threads
{

    public interface ICThreadPool
    {
        CTask<T> QueueWorkItem<T>(Func<T, int> act);

        int MaxConcurrentCount { set; get; }

        int WorkItemTimeoutMiliseconds { set; get; }
    }
}
