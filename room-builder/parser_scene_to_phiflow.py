import json


def calculate_room_and_objects(data):

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

            # Apply correction only if x or z is less than 0
            adjusted_x_min = coords['x'] - min_x if coords['x'] < 0 \
                    else coords['x']
            adjusted_z_min = coords['z'] - min_z if coords['z'] < 0 \
                    else coords['z']
            adjusted_y_min = coords['y'] if coords['y'] <= 2.5 \
                    else 2.5 - dimensions['y']  # Ensure height does not exceed 2.5

            adjusted_x_max = adjusted_x_min + dimensions['x'] \
                    if adjusted_x_min + dimensions['x'] < \
                    room_dimensions['x'] else room_dimensions['x']
            adjusted_y_max = adjusted_y_min + dimensions['y'] \
                    if adjusted_y_min + dimensions['y'] < \
                    room_dimensions['y'] else room_dimensions['y']
            adjusted_z_max = adjusted_z_min + dimensions['z'] \
                    if adjusted_z_min + dimensions['z'] < \
                    room_dimensions['z'] else room_dimensions['z']

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
