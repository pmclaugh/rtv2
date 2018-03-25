# CLIVE

## Overview
<p>Both technology and our understanding of the world are rapidly growing, and one of the most obvious signs of this is the
rapid advancement of Computer Graphics (CG). We see it in movies, games, virtual reality, etc. Essentially every form of digital entertainment aims to captivate its audience by immersing them in worlds that grow harder and harder to distinguish from reality. The industry continues to push these boundaries every day, and the lines between these simulated worlds and our reality are beginning to blur.</p>
<p>We are CLIVE (Computed Light in Virtual Evironments) and we have set out to build our own photorealstic renderer from the
ground up. Along our journey, we will share with you all of the struggles and breakthroughs as we trudge the treachorous path of academic papers and PhD theses that litters the world of CG, and build a bridge for all the people interested in this amazing field that don't have the industry experience they need to get off the ground.</p>

## Install
1. <i class="icon-folder-open"></i> Navigate to the desired directory and run in your shell:
`git clone https://github.com/pmclaugh/rtv2.git`
2. <i class="icon-folder-open"></i> Enter the repository you just cloned and run:
`make`

## Features
### Global Illumination
- Environments globally illuminated via volume lighting
- Talk about ray tracing. This section might get a bit long if detailed.
### Highly optimized acceleration structure
- BVH blabber
### Super fine micro-facet surfacing
- GGX blurbs
### Robust file import
- (if we continue adding to this)
### Interactive step-through environment view mode
- Movement Controls: move and turn through your scene in real-time finding the best shot, having access to valuable   information along the way.
  - <kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> keys move you forward, left, back, and right respectively (relative to camera view)
  - <kbd>SPACE</kbd> and <kbd>LSHIFT</kbd> move you up and down respectively along the y-axis (not relative to camera)
- 4 modes to cycle through:
  1. Standard flat-shaded view - Find the best angle for your scene.
  2. Polygon view - Visualize all of the primary shapes the compose the scene.
  3. Standard BVH view - View the acceleration structure interactively, in real-time.
  4. BVH Heatmap view - Alternative BVH view showing the structure with a heatmap color scheme.
- Cycle through modes using the <kbd>TAB</kbd> key.

This project is a work in progress. We have weekly updates on our blog: [link here] as well as a github webpage: [link here]
