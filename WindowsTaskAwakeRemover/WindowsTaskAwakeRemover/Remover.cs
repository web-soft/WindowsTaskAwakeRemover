using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Win32.TaskScheduler;

namespace WindowsTaskAwakeRemover
{
    public class Remover
    {
        private int _nbRemovedAwake =0;
        public Remover()
        {
            using (TaskService ts = new TaskService())
            {
                var tasks = ts.FindAllTasks(t => t.Definition.Settings.WakeToRun).ToList();
                foreach (var t in tasks)
                {
                    Console.WriteLine(t.Name);
                    t.Definition.Settings.WakeToRun = false;
                    t.RegisterChanges();
                    _nbRemovedAwake++;
                }
            }
            if (_nbRemovedAwake == 0)
                Console.WriteLine("No schedule Tasks can awake you computer found");
            else
                Console.WriteLine(_nbRemovedAwake + "Schedule Tasks will not awake you computer anymore :)");
        }
    }
}
