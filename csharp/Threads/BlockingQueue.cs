using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace XXX.Threads
{
    public class BlockingQueue<T>
    {
        private readonly Queue<T> queue = new Queue<T>();
        //private readonly int maxSize;
        //public BlockingQueue(int maxSize) { this.maxSize = maxSize; }

        public void Enqueue(T item)
        {
            lock (queue)
            {
                /*
                while (queue.Count >= maxSize)
                {
                    Monitor.Wait(queue);
                }*/
                queue.Enqueue(item);
                if (queue.Count == 1)
                {
                    // wake up any blocked dequeue
                    Monitor.Pulse(queue);
                }
            }
        }

        public T Dequeue()
        {
            lock (queue)
            {
                while (queue.Count == 0)
                {
                    Monitor.Wait(queue);
                }
                T item = queue.Dequeue();
                /*
                if (queue.Count == maxSize - 1)
                {
                    // wake up any blocked enqueue
                    Monitor.PulseAll(queue);
                }*/
                return item;
            }
        }

        public T Poll(int millisecondsTimeout)
        {
            lock (queue)
            {
                bool timeout = false;
                while (queue.Count == 0 && !timeout)
                {
                    timeout = !Monitor.Wait(queue, millisecondsTimeout);
                }
                if (queue.Count > 0)
                {
                    T item = queue.Dequeue();
                    /*
                    if (queue.Count == maxSize - 1)
                    {
                        // wake up any blocked enqueue
                        Monitor.PulseAll(queue);
                    }*/
                    return item;
                }
                else
                {
                    return default(T);
                }
            }
        }

        public int Size
        {
            get
            {
                return queue.Count;
            }
        }
    }
}
