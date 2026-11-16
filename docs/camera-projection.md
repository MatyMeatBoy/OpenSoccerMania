# Camera projection and zoom

Camera zoom and experimental Hor+ are independent. On a supported, verified
executable the camera hook is installed for both modes, allowing the settings
panel to change zoom during play even when Hor+ is disabled.

With `ExperimentalHorPlus=0`, default zoom uses the untouched original camera
builder. Optional zoom retains the camera's source aspect ratio and adjusts only
its field of view. The 0.85 and 0.70 choices widen the view, including at 4:3.
They do not change camera position or automatically enable aspect correction.

With `ExperimentalHorPlus=1`, the lens also uses the output aspect ratio to keep
vertical framing consistent. Zoom multiplies the projected vertical scale by
0.85 or 0.70 after that aspect correction. Camera lens inputs are restored after
the original builder copies its active projection, preventing cumulative zoom.

Snapshot allocation/save hooks remain independent of the smoothing switch. A
newly allocated previous bank is seeded from the first complete save once;
subsequent saves keep distinct endpoints. Toggling smoothing does not reset or
reseed those banks.

Native projection tests check stock framing across output aspects, zoom in 4:3
and non-Hor+ modes, Hor+ vertical-framing equivalence, and invalid inputs. Native
interpolation tests cover first-bank initialization, distinct endpoints across subsequent saves, and allocation reuse. Source review
confirms that the smoothing toggle does not control snapshot save/priming. These are mathematical/state tests;
physical gameplay camera appearance remains a separate verification step.
