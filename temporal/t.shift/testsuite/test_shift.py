"""Test t.shift

(C) 2015 by the GRASS Development Team
This program is free software under the GNU General Public
License (>=v2). Read the file COPYING that comes with GRASS
for details.

:authors: Soeren Gebbert
"""

import os

import grass.temporal as tgis
from grass.gunittest.case import TestCase


class TestShiftAbsoluteSTRDS(TestCase):
    @classmethod
    def setUpClass(cls):
        """Initiate the temporal GIS and set the region"""
        os.putenv("GRASS_OVERWRITE", "1")
        tgis.init()
        cls.use_temp_region()
        cls.runModule("g.region", s=0, n=80, w=0, e=120, b=0, t=50, res=10, res3=10)
        cls.runModule("r.mapcalc", expression="a1 = 100", overwrite=True)
        cls.runModule("r.mapcalc", expression="a2 = 200", overwrite=True)
        cls.runModule("r.mapcalc", expression="a3 = 300", overwrite=True)
        cls.runModule("r.mapcalc", expression="a4 = 400", overwrite=True)

        cls.runModule(
            "t.create",
            type="strds",
            temporaltype="absolute",
            output="A",
            title="A test",
            description="A test",
            overwrite=True,
        )

        cls.runModule(
            "t.register",
            type="raster",
            input="A",
            maps="a1,a2,a3,a4",
            start="2001-01-01",
            increment="14 days",
            overwrite=True,
        )

    @classmethod
    def tearDownClass(cls):
        """Remove the temporary region"""
        cls.del_temp_region()
        cls.runModule("t.remove", flags="df", type="strds", inputs="A")

    def test_1(self):
        A = tgis.open_old_stds("A", type="strds")
        A.select()
        self.assertEqual(A.get_map_time(), "point")
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2001)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="1 day", type="strds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2001)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 2)

        self.assertModule("t.shift", input="A", granularity="-1 day", type="strds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2001)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="-1 year", type="strds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="6 month", type="strds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 7)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="1 hour", type="strds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 7)
        self.assertEqual(start.day, 1)
        self.assertEqual(start.hour, 1)

        self.assertModule(
            "t.shift", input="A", granularity="-3630 seconds", type="strds"
        )

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 6)
        self.assertEqual(start.day, 30)
        self.assertEqual(start.hour, 23)
        self.assertEqual(start.minute, 59)
        self.assertEqual(start.second, 30)


class TestShiftRelativeSTRDS(TestCase):
    @classmethod
    def setUpClass(cls):
        """Initiate the temporal GIS and set the region"""
        os.putenv("GRASS_OVERWRITE", "1")
        tgis.init()
        cls.use_temp_region()
        cls.runModule("g.region", s=0, n=80, w=0, e=120, b=0, t=50, res=10, res3=10)
        cls.runModule("r.mapcalc", expression="a1 = 100", overwrite=True)
        cls.runModule("r.mapcalc", expression="a2 = 200", overwrite=True)
        cls.runModule("r.mapcalc", expression="a3 = 300", overwrite=True)
        cls.runModule("r.mapcalc", expression="a4 = 400", overwrite=True)

        cls.runModule(
            "t.create",
            type="strds",
            temporaltype="relative",
            output="A",
            title="A test",
            description="A test",
            overwrite=True,
        )

        cls.runModule(
            "t.register",
            type="raster",
            input="A",
            maps="a1,a2,a3,a4",
            start="0",
            increment="14",
            unit="days",
            overwrite=True,
        )

    @classmethod
    def tearDownClass(cls):
        """Remove the temporary region"""
        cls.del_temp_region()
        cls.runModule("t.remove", flags="df", type="strds", inputs="A")

    def test_1(self):
        A = tgis.open_old_stds("A", type="strds")
        A.select()
        self.assertEqual(A.get_map_time(), "point")
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start, 0)

        self.assertModule("t.shift", input="A", granularity="1", type="strds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start, 1)


class TestShiftAbsoluteSTR3DS(TestCase):
    @classmethod
    def setUpClass(cls):
        """Initiate the temporal GIS and set the region"""
        os.putenv("GRASS_OVERWRITE", "1")
        tgis.init()
        cls.use_temp_region()
        cls.runModule("g.region", s=0, n=80, w=0, e=120, b=0, t=50, res=10, res3=10)
        cls.runModule("r3.mapcalc", expression="a1 = 100", overwrite=True)
        cls.runModule("r3.mapcalc", expression="a2 = 200", overwrite=True)
        cls.runModule("r3.mapcalc", expression="a3 = 300", overwrite=True)
        cls.runModule("r3.mapcalc", expression="a4 = 400", overwrite=True)

        cls.runModule(
            "t.create",
            type="str3ds",
            temporaltype="absolute",
            output="A",
            title="A test",
            description="A test",
            overwrite=True,
        )

        cls.runModule(
            "t.register",
            type="raster_3d",
            input="A",
            maps="a1,a2,a3,a4",
            start="2001-01-01",
            increment="14 days",
            overwrite=True,
        )

    @classmethod
    def tearDownClass(cls):
        """Remove the temporary region"""
        cls.del_temp_region()
        cls.runModule("t.remove", flags="df", type="str3ds", inputs="A")

    def test_1(self):
        A = tgis.open_old_stds("A", type="str3ds")
        A.select()
        self.assertEqual(A.get_map_time(), "point")
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2001)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="1 day", type="str3ds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2001)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 2)

        self.assertModule("t.shift", input="A", granularity="-1 day", type="str3ds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2001)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="-1 year", type="str3ds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="6 month", type="str3ds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 7)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="1 hour", type="str3ds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 7)
        self.assertEqual(start.day, 1)
        self.assertEqual(start.hour, 1)

        self.assertModule(
            "t.shift", input="A", granularity="-3630 seconds", type="str3ds"
        )

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 6)
        self.assertEqual(start.day, 30)
        self.assertEqual(start.hour, 23)
        self.assertEqual(start.minute, 59)
        self.assertEqual(start.second, 30)


class TestShiftRelativeSTR3DS(TestCase):
    @classmethod
    def setUpClass(cls):
        os.putenv("GRASS_OVERWRITE", "1")
        tgis.init()
        cls.use_temp_region()
        cls.runModule("g.region", s=0, n=80, w=0, e=120, b=0, t=50, res=10, res3=10)
        cls.runModule("r3.mapcalc", expression="a1 = 100", overwrite=True)
        cls.runModule("r3.mapcalc", expression="a2 = 200", overwrite=True)
        cls.runModule("r3.mapcalc", expression="a3 = 300", overwrite=True)
        cls.runModule("r3.mapcalc", expression="a4 = 400", overwrite=True)

        cls.runModule(
            "t.create",
            type="str3ds",
            temporaltype="relative",
            output="A",
            title="A test",
            description="A test",
            overwrite=True,
        )

        cls.runModule(
            "t.register",
            type="raster_3d",
            input="A",
            maps="a1,a2,a3,a4",
            start="0",
            increment="14",
            unit="days",
            overwrite=True,
        )

    @classmethod
    def tearDownClass(cls):
        """Remove the temporary region"""
        cls.del_temp_region()
        cls.runModule("t.remove", flags="df", type="str3ds", inputs="A")

    def test_1(self):
        A = tgis.open_old_stds("A", type="str3ds")
        A.select()
        self.assertEqual(A.get_map_time(), "point")
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start, 0)

        self.assertModule("t.shift", input="A", granularity="1", type="str3ds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start, 1)


class TestShiftAbsoluteSTVDS(TestCase):
    @classmethod
    def setUpClass(cls):
        """Initiate the temporal GIS and set the region"""
        os.putenv("GRASS_OVERWRITE", "1")
        tgis.init()
        cls.use_temp_region()
        cls.runModule("g.region", s=0, n=80, w=0, e=120, b=0, t=50, res=10, res3=10)
        cls.runModule("v.random", quiet=True, npoints=20, seed=1, output="a1")
        cls.runModule("v.random", quiet=True, npoints=20, seed=1, output="a2")
        cls.runModule("v.random", quiet=True, npoints=20, seed=1, output="a3")
        cls.runModule("v.random", quiet=True, npoints=20, seed=1, output="a4")

        cls.runModule(
            "t.create",
            type="stvds",
            temporaltype="absolute",
            output="A",
            title="A test",
            description="A test",
            overwrite=True,
        )

        cls.runModule(
            "t.register",
            type="vector",
            input="A",
            maps="a1,a2,a3,a4",
            flags="i",
            start="2001-01-01",
            increment="14 days",
            overwrite=True,
        )

    @classmethod
    def tearDownClass(cls):
        """Remove the temporary region"""
        cls.del_temp_region()
        cls.runModule("t.remove", flags="df", type="stvds", inputs="A")

    def test_1(self):
        A = tgis.open_old_stds("A", type="stvds")
        A.select()
        self.assertEqual(A.get_map_time(), "interval")
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2001)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="1 day", type="stvds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2001)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 2)

        self.assertModule("t.shift", input="A", granularity="-1 day", type="stvds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2001)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="-1 year", type="stvds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 1)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="6 month", type="stvds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 7)
        self.assertEqual(start.day, 1)

        self.assertModule("t.shift", input="A", granularity="1 hour", type="stvds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 7)
        self.assertEqual(start.day, 1)
        self.assertEqual(start.hour, 1)

        self.assertModule(
            "t.shift", input="A", granularity="-3630 seconds", type="stvds"
        )

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start.year, 2000)
        self.assertEqual(start.month, 6)
        self.assertEqual(start.day, 30)
        self.assertEqual(start.hour, 23)
        self.assertEqual(start.minute, 59)
        self.assertEqual(start.second, 30)


class TestShiftRelativeSTVDS(TestCase):
    @classmethod
    def setUpClass(cls):
        """Initiate the temporal GIS and set the region"""
        os.putenv("GRASS_OVERWRITE", "1")
        tgis.init()
        cls.use_temp_region()
        cls.runModule("g.region", s=0, n=80, w=0, e=120, b=0, t=50, res=10, res3=10)
        cls.runModule("v.random", quiet=True, npoints=20, seed=1, output="a1")
        cls.runModule("v.random", quiet=True, npoints=20, seed=1, output="a2")
        cls.runModule("v.random", quiet=True, npoints=20, seed=1, output="a3")
        cls.runModule("v.random", quiet=True, npoints=20, seed=1, output="a4")

        cls.runModule(
            "t.create",
            type="stvds",
            temporaltype="relative",
            output="A",
            title="A test",
            description="A test",
            overwrite=True,
        )

        cls.runModule(
            "t.register",
            type="vector",
            input="A",
            maps="a1,a2,a3,a4",
            start="0",
            flags="i",
            increment="14",
            unit="days",
            overwrite=True,
        )

    @classmethod
    def tearDownClass(cls):
        """Remove the temporary region"""
        cls.del_temp_region()
        cls.runModule("t.remove", flags="df", type="stvds", inputs="A")

    def test_1(self):
        A = tgis.open_old_stds("A", type="stvds")
        A.select()
        self.assertEqual(A.get_map_time(), "interval")
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start, 0)

        self.assertModule("t.shift", input="A", granularity="1", type="stvds")

        A.select()
        start, end = A.get_temporal_extent_as_tuple()
        self.assertEqual(start, 1)


class TestShiftAbsoluteError(TestCase):
    @classmethod
    def setUpClass(cls):
        """Initiate the temporal GIS and set the region"""
        os.putenv("GRASS_OVERWRITE", "1")
        tgis.init()
        cls.use_temp_region()
        cls.runModule("g.region", s=0, n=80, w=0, e=120, b=0, t=50, res=10, res3=10)
        cls.runModule("r.mapcalc", expression="a1 = 100", overwrite=True)
        cls.runModule("r.mapcalc", expression="a2 = 200", overwrite=True)
        cls.runModule("r.mapcalc", expression="a3 = 300", overwrite=True)
        cls.runModule("r.mapcalc", expression="a4 = 400", overwrite=True)

        cls.runModule(
            "t.create",
            type="strds",
            temporaltype="relative",
            output="A",
            title="A test",
            description="A test",
            overwrite=True,
        )

        cls.runModule(
            "t.register",
            type="raster",
            input="A",
            maps="a1,a2,a3,a4",
            start="0",
            increment="14",
            unit="days",
            overwrite=True,
        )

    @classmethod
    def tearDownClass(cls):
        """Remove the temporary region"""
        cls.del_temp_region()
        cls.runModule("t.remove", flags="df", type="strds", inputs="A")

    def test_1(self):
        pass
        # self.assterModuleFail()


if __name__ == "__main__":
    from grass.gunittest.main import test

    test()
