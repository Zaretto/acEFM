# acEFM
This permits the use of JSBSim models from with DCS World;
There must be a config file in the root of your mod; "aceFMconfig.xml" that sets the basic data (properties) and defines which JSBSim XML file to use. Usually the JSBSim XML will include other files (e.g. engines, systems).

* Required to use my JSBSim fork https://github.com/Zaretto/jsbsim.git using branch DCS-WIP-hacky 
* See https://github.com/Zaretto/DCS-SEPECAT-Jaguar for an example

## acEFMconfig.xml DCS elements

### Cockpit API

acEFM supports the mapping between properties and the cockpit API `(pfn_ed_cockpit_update_parameter_with_number(Handle, val);` 

Nodes as follows
* <param> node defines the Handle to lookup
* <property> where the value comes from
* <factor> optional fixed factor to apply
* <delta> the amount the property must change before an update is trigged (optional, default 0.0001)
* <type> defines the type of the node which defines how the property value is handled prior to setting the value on the handle. Currently supported is the default type (nothing special) or `GenevaDrive` which will animation a Geneva Drive for instrument drums. `LinearDrive` is a linear drive. Only the default type is currently fully implemented.

```    <!-- bind all cockpit params -->
    <cockpit>
      <gauge>
        <param>Airspeed</param>
        <property>/fdm/jsbsim/velocities/vc-kts</property>
      </gauge>
      
      <gauge>
        <param>FuelFlow_Right</param>
        <property>/fdm/jsbsim/propulsion/engine[1]/fuel-flow-rate-pps</property>
        <factor>3600</factor>
      </gauge>
      ...
    </cockpit>
```

### Animations
The config file can contain an `<animation>` node that permits the mapping of draw arguments

### Draw arguments
You can define which properties are mapped to the **draw arguments** for your model. These will be set inside `ed_fm_set_draw_args` 

Nodes as follows
* <param> node defines the Handle to lookup
* <property> where the value comes from
* <factor> optional fixed factor to apply
* <delta> the amount the property must change before an update is trigged (optional, default 0.0001)

e.g. for afterburners.
```
    <animations>
      <drawarg n="28">
        <property>fdm/jsbsim/propulsion/engine[0]/augmentation-alight-norm</property>
        <delta>0.01</delta>
      </drawarg>
      <drawarg n="29">
        <property>fdm/jsbsim/propulsion/engine[1]/augmentation-alight-norm</property>
        <delta>0.01</delta>
      </drawarg>
    </animations>
```

## Folder structure

The main config files is **c:\users\YOU\Saved Games\DCS.openbeta\Mods\Aircraft\YOURMODEL\aceFMconfig.xml**. This defines all of the basic properties that the JSBSim XML requires and is where you can define what the draw arguments and cockpit animations.

JSBSim XML files 
* EFM/YOURMODEL.xml
* EFM/engines/
* EFM/systems/

e.g.

* efm\Engines
* efm\Systems
* efm\YOURMODEL-main-jsb.xml
* efm\Engines\direct.xml
* efm\Engines\YOURENGINE.xml
* efm\Systems\YOURFCS.xml
* efm\Systems\other-system.xml

# SYMON

Symon permits the inspection and modifications of all properties at run time. Your EFM\jsbsim-model.xml must have the following 
```
 <input port="1137"/>
```
Symon must be connected after DCS has loaded your model (and the debug window has appeared). Once connected you should use the "reload" button to populate the list of properties. Once populated you can double click a property on the left window to include it on the right.

![Symon GUI image.](https://i.imgur.com/QaZcFLd.png)