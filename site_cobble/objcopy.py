import cobble

class Objcopy(cobble.Target):
  def __init__(self, loader, package, name,
               environment,
               src,
               options):
    super(Objcopy, self).__init__(loader, package, name)
    self.environment = environment
    self.src = src
    self.options = options
    self.leaf = True

  def _derive_local(self, unused):
    return self.package.project.named_envs[self.environment]

  def _using_and_products(self, env_local): 
    out = self.package.genpath(self.name)

    converter = {
      'rule': 'objcopy',
      'outputs': [out],
      'inputs': [self.src],
      'variables': {
        'objcopy': env_local['objcopy'],
        'options': self.options,
      },
    }

    using = cobble.env.make_appending_delta(
      __implicit__ = [self.identifier],
    )

    return (using, [converter])

package_verbs = {
  'objcopy': Objcopy,
}

ninja_rules = {
  'objcopy': {
    'command': '$objcopy $options $in $out',
    'description': 'OBJCOPY $out',
  },
}
