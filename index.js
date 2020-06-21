'use strict';
import m from './mithril.js';

let count = 0;

const Hello = {
    view() {
        const els = [
            'Hello world!',
            m('button', { onclick: () => count++ }, count + ' clicks'),
        ];
        for (let i = 0; i < count; i++) {
            els.push('item ' + i);
        }
        return m('vbox', els);
    },
};

m.mount(document.body, Hello);

console.log('finished loading index.js');
