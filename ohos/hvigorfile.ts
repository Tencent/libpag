import { appTasks, OhosAppContext, OhosPluginId } from '@ohos/hvigor-ohos-plugin';
import { hvigor, getNode } from '@ohos/hvigor'
import fs, { close } from "node:fs";
import { resolve, dirname } from 'path';

interface OverrideSigningConfig {
  bundleName?: string;
  signingConfigs?: Record[];
};

function overrideSigningConfigIfNeeded() {
  const configFilePath = resolve(dirname(__filename), 'local.signingconfig.json');
  if (!fs.existsSync(configFilePath)) {
    return;
  }

  const data = fs.readFileSync(configFilePath, { encoding: "utf8" })
  const config = JSON.parse(data) as OverrideSigningConfig
  if (!config.signingConfigs || config.signingConfigs.length == 0) {
    return;
  }


  const rootNode = getNode(__filename);
  rootNode.afterNodeEvaluate(node => {
    const appContext = node.getContext(OhosPluginId.OHOS_APP_PLUGIN) as OhosAppContext;
    const buildProfileOpt = appContext.getBuildProfileOpt();
    const oldSigningConfigs = buildProfileOpt['app']['signingConfigs']
    buildProfileOpt['app']['signingConfigs'] = config.signingConfigs;
    console.log(`override signingConfigs:
======================old=======================
${JSON.stringify(oldSigningConfigs, null, 4)}
======================new=======================
${JSON.stringify(config.signingConfigs, null, 4)}
`);
    appContext.setBuildProfileOpt(buildProfileOpt);
  });

  if (!config.bundleName || config.bundleName == '') {
    return;
  }

  hvigor.getRootNode().afterNodeEvaluate(rootNode => {
    const appContext = rootNode.getContext(OhosPluginId.OHOS_APP_PLUGIN) as OhosAppContext;
    const appJsonOpt = appContext.getAppJsonOpt();
    const oldBundleName = appJsonOpt['app']['bundleName']
    appJsonOpt['app']['bundleName'] = config.bundleName;
    console.log(`override bundleName: ${oldBundleName} ~> ${config.bundleName}`);
    appContext.setAppJsonOpt(appJsonOpt);
  });
}

overrideSigningConfigIfNeeded();

export default {
  system: appTasks, /* Built-in plugin of Hvigor. It cannot be modified. */
  plugins: [], /* Custom plugin to extend the functionality of Hvigor. */
}

