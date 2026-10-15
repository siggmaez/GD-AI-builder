import unittest
import bridge
from object_params import properties,palette,validate_links


class ObjectParameterTests(unittest.TestCase):
    def test_palette_groups_layers_and_move_survive_response_validation(self):
        ctx=bridge.context({"prompt":"Decorate","mode":"decor"})
        shape={"id":693,"x":75,"y":75,"groups":[1],"base_color":1,"scale_x":2,"scale_y":.5,"flip_x":True,"z_layer":-1,"z_order":3}
        move={"id":901,"x":15,"y":15,"trigger":{"type":"move","target_group":1,"move_y":30,"duration":1}}
        result=bridge.validate_objects({"objects":[shape,move],"palette":[{"id":1,"r":40,"g":50,"b":60}]},ctx)
        self.assertEqual(result["objects"][0]["groups"],[1])
        self.assertEqual(result["objects"][1]["trigger"]["move_y"],30)
        self.assertEqual(result["palette"][0]["r"],40)

    def test_unconfigured_trigger_and_raw_properties_rejected(self):
        for obj in ({"id":901},{"id":901,"trigger":{"type":"move","target_group":1,"raw":"57,9"}},
                    {"id":899,"trigger":{"type":"move","target_group":1}}):
            with self.assertRaises(ValueError):properties(obj)

    def test_links_cannot_target_unassigned_groups_or_colors(self):
        for obj in ({"id":1,"base_color":99},{"id":901,"trigger":{"target_group":99}}):
            with self.assertRaises(ValueError):validate_links([obj],[])

    def test_spawn_cycle_rejected(self):
        objects=[{"id":1268,"groups":[1],"trigger":{"type":"spawn","target_group":2}},
                 {"id":1268,"groups":[2],"trigger":{"type":"spawn","target_group":1}}]
        with self.assertRaises(ValueError):validate_links(objects,[])

    def test_invalid_rgb_and_nonfinite_parameters_rejected(self):
        with self.assertRaises(ValueError):palette([{"id":1,"r":256,"g":0,"b":0}])
        with self.assertRaises(ValueError):properties({"id":693,"scale_x":float("nan")})
        with self.assertRaises(ValueError):properties({"id":693,"groups":[True]})
