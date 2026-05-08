# GRF Examples

> use -DGRF_EXAMPLES=ON when generating the build files with CMake to build the examples

Located in the `examples` folder in the GRF repository you will find a few biggish projects that are meant not only to stress test the framework but also provide an example of what one can do with the framework. Below is a list of the examples with descriptions and references for further reading.

### N-Body Simulation

A newtonian gravity simulation of cosmic dust using a LBVH tree to simulate massive amounts of particles. Dragging your mouse spawns new particles into the scene. A UI panel shows particle count, an AABB debug visual toggle, and a button to reset the simulation.

LBVH construction and radix sort algorithms were adapted to fit GSL from MicroWerner's implementation (linked below).

**References**
- [Thinking Parallel III](https://developer.nvidia.com/blog/thinking-parallel-part-iii-tree-construction-gpu/)
- [Karras 2012](https://research.nvidia.com/sites/default/files/pubs/2012-06_Maximizing-Parallelism-in/karras2012hpg_paper.pdf)
- [VkLBVH](https://github.com/MircoWerner/VkLBVH)
- [VkRadixSort](https://github.com/MircoWerner/VkRadixSort)
- [ToruNiina/lbvh](https://github.com/ToruNiina/lbvh)
