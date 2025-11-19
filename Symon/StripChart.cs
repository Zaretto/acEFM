using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Symon.DataSource
{
    public class StripChart : Control
    {
        private readonly List<(DateTime time, double value)> _data = new List<(DateTime time, double value)>();
        private double _minY, _maxY;

        public DataItem DataItem { get; }
        public int DisplaySeconds { get; }
        private bool _selected;

        public bool Selected
        {
            get => _selected;
            set
            {
                if (_selected != value)
                {
                    _selected = value;
                    Invalidate(); // repaint to show selection
                }
            }
        }

        public StripChart(int height, DockStyle dock, DataItem dataItem, int displaySeconds)
        {
            Height = height;
            Dock = dock;
            DataItem = dataItem;
            DoubleBuffered = true;
            DisplaySeconds = displaySeconds;
            // Subscribe to property changes
            AddValue(dataItem.GetDoubleValue());
            AddValue(dataItem.GetDoubleValue());
            dataItem.PropertyChanged += (s, e) =>
            {
                if (e.PropertyName == nameof(DataItem.Value))
                {
                    var v = dataItem.GetDoubleValue();
                    if (v.HasValue)
                    {
                        AddValue(v.Value);
                        Invalidate();
                    }
                }
            };
            Invalidate();
            // Subscribe to click
            //this.MouseClick += (s, e) => Selected = !Selected;
        }

        public void AddValue(double? value)
        {
            if (value.HasValue)
                _data.Add((DateTime.Now, value.Value));
        }

        public void DrawChart(Graphics g, Rectangle rect, DateTime cutoff, int displaySeconds)
        {
            if (_data.Any())
            {
                string yTopLabel = "";
                string yBottomLabel = "";
                
                var visible = _data.Where(p => p.time >= cutoff).ToList();
                if (visible.Count > 2)
                {

                    _minY = visible.Min(p => p.value);
                    _maxY = visible.Max(p => p.value);
                    if (_minY == _maxY) { _minY -= 1; _maxY += 1; }

                    var pen = new Pen(Color.Blue, 2);
                    PointF? lastPoint = null;

                    foreach (var p in visible)
                    {
                        float x = rect.Left + (float)((p.time - cutoff).TotalSeconds / displaySeconds) * rect.Width;
                        float y = rect.Bottom - (float)((p.value - _minY) / (_maxY - _minY)) * rect.Height;

                        var pt = new PointF(x, y);
                        if (lastPoint != null)
                            g.DrawLine(pen, lastPoint.Value, pt);

                        lastPoint = pt;
                    }
                    yTopLabel = $"{_maxY:F2}";
                    yBottomLabel = $"{_minY:F2}";
                    g.DrawString(yBottomLabel, Font, Brushes.Black, rect.Left, rect.Bottom - Font.Height);
                    g.DrawString(yTopLabel, Font, Brushes.Black, rect.Left, rect.Top);
                }
                g.DrawRectangle(Selected ? Pens.Red : Pens.Black, rect);
                if (!string.IsNullOrEmpty(DataItem.Name))
                {
                    var d = g.MeasureString(DataItem.Name, Font);

                    StringFormat format = new StringFormat();
                    format.Trimming = StringTrimming.EllipsisPath; // or StringTrimming.None
                    format.FormatFlags = StringFormatFlags.NoWrap;
                    var labelLen = g.MeasureString(yTopLabel, Font).Width;
                    RectangleF drawRect = new RectangleF(Math.Max(rect.Left + labelLen, rect.Right - d.Width), rect.Top, rect.Width-labelLen, rect.Height);
                    g.DrawString(DataItem.Name, Font, Brushes.Black, drawRect, format);
                }
            }
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            var cutoff = DateTime.Now.AddSeconds(-DisplaySeconds);
            if (_data.Any())
                cutoff = _data.Max(xx => xx.time).AddSeconds(-DisplaySeconds);
            var paintRectangle = ClientRectangle;
            paintRectangle.Height -= 10;
            paintRectangle.Width -= 1;
            DrawChart(e.Graphics, paintRectangle, cutoff, DisplaySeconds);

        }
    }
}