import json
from typing import Dict
from typing import Any


def calculate_room_and_objects(data: Dict[str, Any]) -> Dict[str, Any]:
    """
    Calculate room dimensions based on the wall coordinates and adjust
    the position and size of objects within the room while ensuring
    height does not exceed 2.5 units. Corrections are applied only to
    objects whose keys start with 'cooler', 'chair', or 'table' and
    where 'x' or 'z' coordinates are less than 0.

    Args:
        data (Dict[str, Any]): A dictionary containing object data, where
            each object has keys for 'coords' and 'dimensions'. Wall
            objects are used to calculate room size, and object positions
            are adjusted based on room constraints.

    Returns:
        Dict[str, Any]: A dictionary containing the calculated room
            dimensions and the adjusted object coordinates.
    """
    # Extract walls' coordinates
    walls = {key: value['coords'] for key, value in data.items()
             if 'wall' in key}

    # Find min and max values for x and z from walls
    min_x = min(wall['x'] for wall in walls.values())
    max_x = max(wall['x'] for wall in walls.values())
    min_z = min(wall['z'] for wall in walls.values())
    max_z = max(wall['z'] for wall in walls.values())

    # Calculate room dimensions
    room_dimensions = {
        'x': round(max_x - min_x, 2),
        'y': 2.5,  # Room height is always 2.5
        'z': round(max_z - min_z, 2)
    }

    # Adjust object positions and ensure height is within limits
    adjusted_objects = {}
    for key, value in data.items():
        if any(prefix in key for prefix in ['cooler', 'chair', 'table']):
            # Adjust x, z and calculate height for these objects
            coords = value['coords']
            dimensions = value['dimensions']

            # Adjust x and z coordinates if less than 0
            adjusted_x_min = max(coords['x'] - min_x, 0)
            adjusted_z_min = max(coords['z'] - min_z, 0)

            # Ensure height does not exceed 2.5 units
            adjusted_y_min = min(coords['y'], 2.5 - dimensions['y'])

            adjusted_x_max = min(adjusted_x_min + dimensions['x'],
                                 room_dimensions['x'])
            adjusted_y_max = min(adjusted_y_min + dimensions['y'],
                                 room_dimensions['y'])
            adjusted_z_max = min(adjusted_z_min + dimensions['z'],
                                 room_dimensions['z'])

            adjusted_objects[key] = {

                    'x': (round(adjusted_x_min, 2),
                          round(adjusted_x_max, 2)),
                    'y': (round(adjusted_y_min, 2),
                          round(adjusted_y_max, 2)),
                    'z': (round(adjusted_z_min, 2),
                          round(adjusted_z_max, 2))

            }

    return {
        'room_dimensions': room_dimensions,
        'adjusted_objects': adjusted_objects
    }


def main():
    with open('test.json', 'r') as f:
        _data = json.load(f)

    result = calculate_room_and_objects(_data)
    return result


if "__main__" == __name__:

    result = main()
    print(result)
