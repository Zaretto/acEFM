using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Symon.DataSource
{
    using System;
    using System.Collections.Generic;
    using System.Drawing;
    using System.Windows.Forms;

    public class StripChartContainer : Panel
    {
        private readonly Timer _timer;
        private int _displaySeconds = 60;

        public int DisplaySeconds
        {
            get => _displaySeconds;
            set { if (value > 0) _displaySeconds = value; }
        }

        public StripChartContainer()
        {
            DoubleBuffered = true;
            AutoScroll = true;
            BackColor = Color.White;
            this.KeyDown += StripChartContainer_KeyDown;
        }
        private void StripChartContainer_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Delete)
            {
                foreach (var c in Controls.OfType<StripChart>().Where(xx => xx.Selected).ToList())
                {
                    Controls.Remove(c);
                    c.Dispose();
                }
            }
        }
        public void AddDataItem(DataItem item)
        {
            var chart = new StripChart(height: 70, dock : DockStyle.Top, dataItem: item, displaySeconds: _displaySeconds);
            Controls.Add(chart);
            chart.MouseClick += (s, e) =>
            {
                chart.Selected = !chart.Selected;
                this.Focus(); // move focus to container so it receives key events
            };

            chart.Invalidate();
            Invalidate();
//            Controls.SetChildIndex(chart, 0);

         }

        internal void Clear()
        {
            Controls.Clear();
        }

        internal bool IsPlotted(string name)
        {
            //var props = Controls.OfType<StripChart>().Select(xx=>xx.DataItem.Name)
            return Controls.OfType<StripChart>().Any(xx=>xx.DataItem.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
        }
    }
}
