
BLENDER=blender

all : \
	../dist/basketball.pnct \
	../dist/basketball.scene \

../dist/basketball.pnct : basketball.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- basketball.blend '$@'

../dist/basketball.scene : basketball.blend export-scene.py
	$(BLENDER) --background --python export-scene.py -- basketball.blend '$@'
