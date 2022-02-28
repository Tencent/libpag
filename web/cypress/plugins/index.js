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
const pako = require('pako');

const downloadsDir = path.join(__dirname, '../downloads/');
const imageHeader = 'data:image/png;base64,';

/**
 * @type {Cypress.PluginConfig}
 */
// eslint-disable-next-line no-unused-vars
module.exports = (on, config) => {
  on('task', {
    readImage(filename) {
      const filePath = path.join(downloadsDir, filename);
      if (fs.existsSync(filePath)) {
        const compressed = fs.readFileSync(filePath);
        console.log('readFileSync succ');
        try {
          const result = pako.inflate(compressed, { to: 'string' });
          console.log('succ');
          return result;
        } catch (e) {
          console.log(e);
          return null;
        }
      }
      return null;
    },
    saveImage({ filename, data }) {
      const output = pako.deflate(data);
      fs.writeFileSync(path.join(downloadsDir, filename), output);
      return null;
    },
  });
  // `on` is used to hook into various events Cypress emits
  // `config` is the resolved Cypress config
};
