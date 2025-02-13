// @ts-check
// Note: type annotations allow type checking and IDEs autocompletion

const path = require('node:path');

const tqlGrammar = require("vscode-tql/syntaxes/tql.tmLanguage.json");

async function createConfig() {
  const rehypePrettyCode = (await import('rehype-pretty-code')).default;
  const shikiHighlighter = (await import('shiki')).createHighlighter;
  const transformerCopyButton = (await import("@rehype-pretty/transformers"))
    .transformerCopyButton;

  const rehypePrettyCodeOptions = {
    keepBackground: false,
    theme: "github-dark-default",
    getHighlighter: (options) =>
      shikiHighlighter({
        ...options,
        langs: [() => tqlGrammar],
      }),
    transformers: [
      transformerCopyButton({
        jsx: true, // required for React, disables the onclick handler, which we add using clientModules
        feedbackDuration: 3_000,
        copyIcon:
          "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxOCIgaGVpZ2h0PSIxOCIgdmlld0JveD0iMCAwIDM2IDM2Ij48cmVjdCB3aWR0aD0iMzYiIGhlaWdodD0iMzYiIGZpbGw9Im5vbmUiLz48cGF0aCBmaWxsPSIjYmZjN2Q1IiBkPSJNMjkuNSA3aC0xOUExLjUgMS41IDAgMCAwIDkgOC41djI0YTEuNSAxLjUgMCAwIDAgMS41IDEuNWgxOWExLjUgMS41IDAgMCAwIDEuNS0xLjV2LTI0QTEuNSAxLjUgMCAwIDAgMjkuNSA3TTI5IDMySDExVjloMThaIiBjbGFzcz0iY2xyLWktb3V0bGluZSBjbHItaS1vdXRsaW5lLXBhdGgtMSIvPjxwYXRoIGZpbGw9IiNiZmM3ZDUiIGQ9Ik0yNiAzLjVBMS41IDEuNSAwIDAgMCAyNC41IDJoLTE5QTEuNSAxLjUgMCAwIDAgNCAzLjV2MjRBMS41IDEuNSAwIDAgMCA1LjUgMjlINlY0aDIwWiIgY2xhc3M9ImNsci1pLW91dGxpbmUgY2xyLWktb3V0bGluZS1wYXRoLTIiLz48cGF0aCBmaWxsPSJub25lIiBkPSJNMCAwaDM2djM2SDB6Ii8+PC9zdmc+",
        successIcon:
          "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxOCIgaGVpZ2h0PSIxOCIgdmlld0JveD0iMCAwIDI0IDI0Ij48cmVjdCB3aWR0aD0iMjQiIGhlaWdodD0iMjQiIGZpbGw9Im5vbmUiLz48cGF0aCBmaWxsPSIjMDBkNjAwIiBkPSJNMjEgN0w5IDE5bC01LjUtNS41bDEuNDEtMS40MUw5IDE2LjE3TDE5LjU5IDUuNTl6Ii8+PC9zdmc+",
      }),
    ],
  };

  /// BEGIN CUSTOM CODE ///

  // This is customized version of
  // https://github.com/alvinometric/remark-inline-svg/blob/e0c3b0ddb3f34a8766b7e39e05a1888a55347f7f/index.js
  // in order to co-operate with Docusaurus.

  // Problem: we store some images in /static/img folder and we want to reference
  // them in markdown as ![image something](/img/something.svg).
  // If we don't want to inline those images this works in docusaurus as docusaurus
  // preprocesses all /img/something.svg correctly). But remark-inline-svg plugin does
  // not support this - it tries to resolve relative to the markdown file.
  // We also need to support relative links like ![image something](something.svg).

  // Solution: we change the plugin's function to rewrite all /img/something.svg
  // to the correct full path which is <ROOT>/static/img/something.svg
  // NOTE: `static` is a default folder for static assets in docusaurus.
  // We provide the option `staticDir` to override this.
  // And in case of relative links we just prepend the path to the markdown file.

  // This is not perfect but it works.

  const inlineSVGTransform = (await import('remark-inline-svg')).transform;
  const visit = (await import('unist-util-visit')).default;

  const inlineSVG = (options = {}) => {
    const suffix = options.suffix || '.inline.svg';
    const staticDir = options.staticDir || 'static';

    return async (tree, file) => {
      const svgs = [];

      visit(tree, 'image', (node) => {
        const {url} = node;

        if (url.endsWith(suffix)) {
          // if url is /img/something.svg, then we need to use our custom logic
          if (url.startsWith('/img/')) {
            const fullPath = path.join(process.cwd(), staticDir, url);
            node.url = fullPath;
          }
          // otherwise we just use the original logic
          else {
            const markdownFileDir = path.dirname(file.history[0]);
            node.url = path.resolve('./', markdownFileDir, url);
          }

          svgs.push(node);
        }
      });

      if (svgs.length > 0) {
        const promises = svgs.map(async (node) => {
          return await inlineSVGTransform(node, options);
        });

        await Promise.all(promises);
      }

      return tree;
    };
  };

  /// END CUSTOM CODE ///
  return {
    title: 'Tenzir',
    tagline: 'Easy data pipelines for security teams.',
    url: 'https://docs.tenzir.com',
    baseUrl: '/',
    onBrokenLinks: 'throw',
    onBrokenMarkdownLinks: 'throw',
    favicon: 'img/tenzir-logo-adaptive.svg',
    organizationName: 'tenzir', // Usually your GitHub org/user name.
    projectName: 'tenzir', // Usually your repo name.
    trailingSlash: false, // GitHub Pages already adds a slash

    markdown: {
      mermaid: true,
    },
    themes: ['@docusaurus/theme-mermaid'],

    clientModules: [require.resolve("./registerCopyButton.js")],
    plugins: [
      'docusaurus-plugin-sass',
      [
        '@docusaurus/plugin-client-redirects',
        {
          redirects: [
            {
              to: '/',
              from: '/docs',
            },
          ],
        },
      ],
    ],

    presets: [
      [
        'classic',
        /** @type {import('@docusaurus/preset-classic').Options} */
        ({
          docs: {
            path: 'docs', // The directory in the filesystem.
            routeBasePath: '/', // Serve the docs at the site's root.
            sidebarPath: require.resolve('./sidebars.js'),
            editUrl: 'https://github.com/tenzir/tenzir/tree/main/web',
            // TODO: The last update author and time is always the person that
            // triggered the last deployment and the time of that deployment.
            // Ideally we'd show this information, but as-is it's unnecessary.
            showLastUpdateTime: false,
            showLastUpdateAuthor: false,
            beforeDefaultRemarkPlugins: [[inlineSVG, {suffix: '.svg'}]],
            rehypePlugins: [[rehypePrettyCode, rehypePrettyCodeOptions]],
            remarkPlugins: [[require('@akopyl/docusaurus-toc-patcher'), {} ]]
          },
          blog: {
            blogTitle: 'Releases',
            blogDescription: 'News from the Tenzir community',
            blogSidebarCount: 20,
            blogSidebarTitle: 'Release Announcements',
            postsPerPage: 20,
            beforeDefaultRemarkPlugins: [[inlineSVG, {suffix: '.svg'}]],
            rehypePlugins: [[rehypePrettyCode, rehypePrettyCodeOptions]],
          },
          pages: {
            beforeDefaultRemarkPlugins: [[inlineSVG, {suffix: '.svg'}]],
          },
          theme: {
            customCss: require.resolve('./src/css/custom.scss'),
          },
          sitemap: {
            changefreq: 'weekly',
            priority: 0.5,
            ignorePatterns: ['/tags/**'],
          },
        }),
      ],
      [
        'redocusaurus',
        {
          specs: [
            {
              spec: 'openapi/openapi.yaml',
              route: '/api/',
            },
          ],
          theme: {
            primaryColor: '#00a4f1',
          },
        },
      ],
    ],

    themeConfig:
      /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
      ({
        algolia: {
          appId: 'BVB58VRBDH',
          apiKey: 'f7c2eb86ff85cd55d9634543ed1c60b2',
          indexName: 'vast',
        },
        colorMode: {
          disableSwitch: false,
          respectPrefersColorScheme: true,
        },
        navbar: {
          logo: {
            alt: 'Tenzir',
            href: 'https://tenzir.com',
            src: 'img/tenzir-black.svg',
            srcDark: 'img/tenzir-white.svg',
            width: 120,
          },
          items: [
            {
              type: 'doc',
              docId: 'overview',
              position: 'left',
              label: 'Docs',
            },
            {
              to: 'https://tenzir.com/blog',
              label: 'Blog',
              position: 'left',
            },
            {
              to: '/blog',
              label: 'Releases',
              position: 'left',
            },
            {
              to: '/changelog',
              label: 'Changelog',
              position: 'left',
            },
            {
              to: '/discord',
              'aria-label': 'Discord',
              className: 'header-discord-link',
              position: 'right',
            },
            {
              href: 'https://github.com/tenzir/tenzir',
              'aria-label': 'GitHub',
              className: 'header-github-link',
              position: 'right',
            },
            {
              type: 'docsVersionDropdown',
              position: 'right',
              dropdownActiveClassDisabled: true,
            },
            {
              type: 'search',
              position: 'right',
            },
          ],
        },
        //announcementBar: {
        //  content:
        //  'Tenzir is now <em>live</em>. Read our <a href="/blog/introducing-tenzir-security-data-pipelines">announcement blog</a> and be among the first to experience it at <a href="https://app.tenzir.com">app.tenzir.com</a>.',
        //  backgroundColor: '#f1f2f2',
        //  isCloseable: true,
        //},
        footer: {
          links: [
            {
              title: 'Resources',
              items: [
                {
                  label: 'Docs',
                  to: '/',
                },
                {
                  label: 'API',
                  to: '/api',
                },
                {
                  label: 'Blog',
                  to: '/blog',
                },
                {
                  label: 'Changelog',
                  to: '/changelog',
                },
                {
                  label: 'Release Notes',
                  href: 'https://docs.google.com/presentation/d/1R59T2nuGJg43g6PVKogZbb6DjcJXRYBx3Bih8W-A-oM/present',
                },
              ],
            },
            {
              title: 'Community',
              items: [
                {
                  label: 'Discord',
                  href: '/discord',
                },
                {
                  label: 'GitHub',
                  href: 'https://github.com/tenzir/tenzir',
                },
                {
                  label: 'Twitter',
                  href: 'https://twitter.com/tenzir_company',
                },
                {
                  label: 'LinkedIn',
                  href: 'https://www.linkedin.com/company/tenzir',
                },
              ],
            },
            {
              title: 'Tenzir',
              items: [
                {
                  label: 'Website',
                  href: 'https://tenzir.com',
                },
                {
                  label: 'Newsletter',
                  to: '/newsletter',
                },
                {
                  label: 'Privacy Statement',
                  to: '/privacy-statement',
                },
                {
                  label: 'Tenzir vs. Cribl',
                  // TODO: remove /next/ after the page is part of a release.
                  href: '/next/tenzir-vs-cribl',
                },
              ],
            },
          ],
          copyright: `Copyright © ${new Date().getFullYear()} Tenzir GmbH.`,
        },
      }),

    // This replaces Docusaurus' default babel loader with esbuild
    // See more at https://github.com/facebook/docusaurus/issues/4765#issuecomment-841135926
    webpack: {
      jsLoader: (isServer) => ({
        loader: require.resolve('esbuild-loader'),
        options: {
          loader: 'tsx',
          format: isServer ? 'cjs' : undefined,
          target: isServer ? 'node12' : 'es2017',
        },
      }),
    },

    scripts: [
      {
        src: 'https://plausible.io/js/script.js',
        aysnc: true,
        defer: true,
        'data-domain': 'docs.tenzir.com',
      },
    ],
  };
}

module.exports = createConfig;
