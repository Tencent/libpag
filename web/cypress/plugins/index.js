/// <reference types="cypress" />
// ***********************************************************
// This example plugins/index.js can be used to load plugins
//
// You can change the location of this file or turn off loading
// the plugins file with the 'pluginsFile' configuration option.
//
// You can read more here:
// https://on.cypress.io/plugins-guide
// ***********************************************************

// This function is called when a project is opened or re-opened (e.g. due to
// the project's config changing)

const fs = require('fs');
const path = require('path');
const brotli = require('brotli');

const baselinesDir = path.join(__dirname, '../baselines/');

/**
 * @type {Cypress.PluginConfig}
 */
// eslint-disable-next-line no-unused-vars
module.exports = (on, config) => {
  on('task', {
    readData(filename) {
      const filePath = path.join(baselinesDir, filename);
      if (fs.existsSync(filePath)) {
        const buffer = brotli.decompress(fs.readFileSync(filePath));
        return buffer;
      }
      return null;
    },
    saveData({ filename, data }) {
      const buffer = Buffer.from(data);
      const output = brotli.compress(buffer);
      fs.writeFileSync(path.join(baselinesDir, filename), Buffer.from(output.buffer));
      return null;
    },
  });
  // `on` is used to hook into various events Cypress emits
  // `config` is the resolved Cypress config
};
