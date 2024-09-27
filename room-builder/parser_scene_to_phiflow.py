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
            adjusted_x = coords['x'] - min_x if coords['x'] < 0 \
                    else coords['x']
            adjusted_z = coords['z'] - min_z if coords['z'] < 0 \
                    else coords['z']
            adjusted_y = coords['y'] if coords['y'] <= 2.5 \
                    else 2.5 - dimensions['y']  # Ensure height does not exceed 2.5

            adjusted_objects[key] = {
                'models_dim': {
                    'x': (round(adjusted_x, 2),
                          round(adjusted_x + dimensions['x'], 2)),
                    'y': (round(adjusted_y, 2),
                          round(adjusted_y + dimensions['y'], 2)),
                    'z': (round(adjusted_z, 2),
                          round(adjusted_z + dimensions['z'], 2))
                },
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
