using Symon.DataSource;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;

public class DataItem : INotifyPropertyChanged
{
    private DataSource ds;
    private string name;
    private string value = "";
    private bool container;
    //private bool plot;
    List<DataItem> Children = null;
    DataItem Parent = null;

    public string Name
    {
        get => name;
        set
        {
            if (name != value)
            {
                // Unsubscribe from old name if it was subscribed
                if (ds is DataSourceJSBSim jsbsim && !string.IsNullOrEmpty(name))
                {
                    jsbsim.OnValueChanged -= OnDataSourceValueChanged;
                }

                name = value;

                // Subscribe to new name
                if (ds is DataSourceJSBSim jsbsimNew && !string.IsNullOrEmpty(name))
                {
                    jsbsimNew.OnValueChanged += OnDataSourceValueChanged;
                }

                OnPropertyChanged();
            }
        }
    }

    public string Value
    {
        get
        {
            return value;
        }
        set
        {
            if (this.value != value)
            {
                this.value = value;
                ds.SetValue(name, value);
                OnPropertyChanged();
            }
        }
    }

    //public bool Plot
    //{
    //    get => plot;
    //    set
    //    {
    //        if (this.plot != value)
    //        {
    //            this.plot = value;
    //            OnPropertyChanged();
    //        }
    //    }
    //}

    // PropertyChanged event and helper method
    public event PropertyChangedEventHandler PropertyChanged;

    protected virtual void OnPropertyChanged([CallerMemberName] string propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }

    // Event handler for DataSource value changes
    private void OnDataSourceValueChanged(string itemName, string newValue)
    {
        if (itemName == this.name && this.value != newValue)
        {
            // Update the value without triggering SetValue on the datasource
            this.value = newValue;
            OnPropertyChanged(nameof(Value));
        }
    }

    public DataItem(DataSource ds, String name)
    {
        this.ds = ds;
        this.name = name;
        container = true;

        // Subscribe to value changes if this is a JSBSim datasource
        if (ds is DataSourceJSBSim jsbsim)
        {
            jsbsim.OnValueChanged += OnDataSourceValueChanged;
        }
    }

    public DataItem(DataSource ds, String name, String val)
    {
        this.ds = ds;
        this.name = name;
        this.value = val;
        this.container = false;

        // Subscribe to value changes if this is a JSBSim datasource
        if (ds is DataSourceJSBSim jsbsim)
        {
            jsbsim.OnValueChanged += OnDataSourceValueChanged;
        }
    }

    // Dispose pattern to unsubscribe from events
    public void Dispose()
    {
        if (ds is DataSourceJSBSim jsbsim)
        {
            jsbsim.OnValueChanged -= OnDataSourceValueChanged;
        }
    }

    public bool IsContainer()
    {
        return container;
    }

    public void SetName(String name)
    {
        this.Name = name;
    }

    public void SetValue(string value)
    {
        this.Value = value;
    }

    public List<DataItem> GetChildren()
    {
        return Children;
    }

    public void AddChildren(List<DataItem> v)
    {
        Children = v;
        for (int i = 0; i < v.Count(); i++)
            v.ElementAt(i).Parent = this;
    }

    public String GetPath()
    {
        String path = "";
        DataItem p = Parent;
        while (p != null)
        {
            path = p.GetName() + path;
            p = p.Parent;
        }
        return path;
    }

    public String GetName()
    {
        return name;
    }

    public string GetValue()
    {
        return value;
    }

    public bool HasChildren()
    {
        return IsContainer();
    }

    public void Update()
    {
        string tv = ds.CurrentValue(Name);
        if (tv != value)
        {
            // Update value directly without triggering SetValue
            this.value = tv;
            OnPropertyChanged(nameof(Value));
        }
    }

    internal void RequestUpdate()
    {
        ds.UpdateValue(Name);
    }
}