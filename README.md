# Dodgeball

[![GitHub release](https://img.shields.io/github/release/allejo/dodgeball.svg)](https://github.com/allejo/dodgeball/releases/latest)
![Minimum BZFlag Version](https://img.shields.io/badge/BZFlag-v2.4.14+-blue.svg)
[![License](https://img.shields.io/github/license/allejo/dodgeball.svg)](LICENSE.md)

Dodgeball is a BZFlag plug-in that introduces a deathmatch-like game mode. Each time a player is killed, they respawn in a "jail" and the only way to escape from jail is to kill an enemy tank while you're in jail. The team that wins the round is the one that manages to place the rest of the entire team in jail.

<table>
    <tr>
        <td colspan="2" align="center">
            <img src=".github/radar.jpg" alt="Radar image of the map">
        </td>
    </tr>
    <tr>
        <td>
            <img src=".github/overview.jpg" alt="Aerial view of the map">
        </td>
        <td>
            <img src=".github/inside_jail.jpg" alt="View of the map from inside a jail">
        </td>
    </tr>
</table>

## Requirements

- BZFlag 2.4.14+
- C++11

## Usage

### Loading the plug-in

This plug-in does not take any specifial configuration options at load time.

```
-loadplugin dodgeball
```

### Custom Map Objects

This plug-in introduces the `JAIL` map object which supports the traditional `position`, `size`, and `rotation` attributes for rectangular objects and `position`, `height`, and `radius` for cylindrical objects.

```text
# Rectangle
jail
  position <x> <y> <z>
  size <x> <y> <z>
  rotation <rot>
  color <team>
end
```

```
# Cylinder
jail
  position <x> <y> <z>
  height <height>
  radius <radius>
  team <team>
end
```

- `team` - The team color that will spawn inside of this jail. Supported values:
  - 1 - Red Team
  - 2 - Green Team
  - 3 - Blue Team
  - 4 - Purple Team

## License

- Plug-in: [MIT](LICENSE.md)
- Map: [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)
