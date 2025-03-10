/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

const program = require('commander');
const path = require('path')
const fs = require('fs-extra')
const config = require('../lib/config')
const util = require('../lib/util')
const build = require('../lib/build')
const versions = require('../lib/versions')
const start = require('../lib/start')
const applyPatches = require('../lib/applyPatches')
const updatePatches = require('./updatePatches')
const pullL10n = require('../lib/pullL10n')
const pushL10n = require('../lib/pushL10n')
const chromiumRebaseL10n = require('../lib/chromiumRebaseL10n')
const l10nDeleteTranslations = require('../lib/l10nDeleteTranslations')
const createDist = require('../lib/createDist')
const test = require('../lib/test')
const gnCheck = require('../lib/gnCheck')
const pylint = require('../lib/pylint')

const collect = (value, accumulator) => {
  accumulator.push(value)
  return accumulator
}

function commaSeparatedList(value, dummyPrevious) {
  return value.split(',')
}

// Use this wrapper function instead of JavaScript's parseInt() with option()
// when defining integer optional parameters, or the default value might get
// passed as well into the radix parameter of parseInt(), causing wrong results.
// https://github.com/brave/brave-browser/issues/13724
function parseInteger(string) {
  // As per the spec [1], not passing the optional radix parameter to parseInt()
  // will make parsing to interpret the string passed as a decimal number unless
  // it's prefixed with '0' (octal) or '0x' (hexadecimal). We only need decimal
  // in this particular case so let's be explicit about that.
  // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/parseInt
  return parseInt(string, 10)
}

const parsedArgs = program.parseOptions(process.argv)

program
  .version(process.env.npm_package_version)

program
  .command('versions')
  .action(versions)

program
  .command('gn_check')
  .option('-C <build_dir>', 'build config (out/Debug, out/Release')
  .option('--target_os <target_os>', 'target OS')
  .option('--target_arch <target_arch>', 'target architecture')
  .option('--target_android_base <target_android_base>', 'target Android OS apk (classic, modern, mono)', 'classic')
  .option('--target_environment <target_environment>', 'target environment (device, catalyst, simulator)')
  .arguments('[build_config]')
  .action(gnCheck)

program
  .command('apply_patches')
  .arguments('[build_config]')
  .action(applyPatches)

program
  .command('update_symlink')
  .option('--symlink_dir <symlink_dir>', 'symlink that points to the actual build directory')
  .option('--target_os <target_os_type>', 'target OS type', /^(host_os|ios|android)$/i)
  .option('--target_arch <target_arch>', 'target architecture', /^(host_cpu|x64|arm64|x86)$/i)
  .arguments('[build_config]')
  .action((buildConfig = config.defaultBuildConfig, options) => {
    config.buildConfig = buildConfig
    if (options.target_os == 'host_os')
      delete options.target_os

    if (options.target_arch == 'host_cpu')
      delete options.target_arch

    config.update(options)
    const current_link = options.symlink_dir
    if (!path.isAbsolute(current_link) && !path.relative(current_link, config.srcDir).startsWith('..')) {
      console.error('Symlink must be an absolute path in src')
      process.exit(1)
    }

    fs.removeSync(current_link)
    fs.symlinkSync(config.outputDir, current_link, 'junction')
    util.generateNinjaFiles()
  })

program
  .command('build')
  .option('-C <build_dir>', 'build config (out/Debug, out/Release')
  .option('--target_os <target_os>', 'target OS')
  .option('--target_arch <target_arch>', 'target architecture')
  .option('--target_android_base <target_android_base>', 'target Android SDK level for apk or aab  (classic, modern, mono)', 'classic')
  .option('--target_android_output_format <target_android_output_format>', 'target Android output format (apk, aab)')
  .option('--target_environment <target_environment>', 'target environment (device, catalyst, simulator)')
  .option('--android_override_version_name <android_override_version_name>', 'Android version number')
  .option('--mac_signing_identifier <id>', 'The identifier to use for signing')
  .option('--mac_signing_keychain <keychain>', 'The identifier to use for signing', 'login')
  .option('--brave_google_api_key <brave_google_api_key>')
  .option('--brave_google_api_endpoint <brave_google_api_endpoint>')
  .option('--brave_infura_project_id <brave_infura_project_id>')
  .option('--bitflyer_client_id <bitflyer_client_id>')
  .option('--bitflyer_client_secret <bitflyer_client_secret>')
  .option('--bitflyer_staging_client_id <bitflyer_staging_client_id>')
  .option('--bitflyer_staging_client_secret <bitflyer_staging_client_secret>')
  .option('--bitflyer_staging_url <bitflyer_staging_url>')
  .option('--uphold_client_id <uphold_client_id>')
  .option('--uphold_client_secret <uphold_client_secret>')
  .option('--uphold_staging_client_id <uphold_staging_client_id>')
  .option('--uphold_staging_client_secret <uphold_staging_client_secret>')
  .option('--channel <target_channel>', 'target channel to build', /^(beta|dev|nightly|release)$/i)
  .option('--ignore_compile_failure', 'Keep compiling regardless of error')
  .option('--skip_signing', 'skip signing binaries')
  .option('--xcode_gen <target>', 'Generate an Xcode workspace ("ios" or a list of semi-colon separated label patterns, run `gn help label_pattern` for more info.')
  .option('--gn <arg>', 'Additional gn args, in the form <key>:<value>', collect, [])
  .option('--ninja <opt>', 'Additional Ninja command-line options, in the form <key>:<value>', collect, [])
  .option('--brave_safetynet_api_key <brave_safetynet_api_key>')
  .option('--is_asan', 'is asan enabled')
  .option('--use_goma [arg]', 'whether to use Goma for building', JSON.parse)
  .option('--goma_offline', 'use offline mode for goma')
  .option('--force_gn_gen', 'always run gn gen')
  .option('--target <target>', 'Custom target to build, instead of the default browser target')
  .option('--build_sparkle', 'Build the Sparkle macOS update framework from source')
  .option('--no_gn_gen', 'Build without running gn gen')
  .arguments('[build_config]')
  .action(build)

program
  .command('create_dist')
  .option('-C <build_dir>', 'build config (out/Debug, out/Release')
  .option('--mac_signing_identifier <id>', 'The identifier to use for signing')
  .option('--mac_installer_signing_identifier <id>', 'The identifier to use for signing the installer')
  .option('--mac_signing_keychain <keychain>', 'The identifier to use for signing', 'login')
  .option('--brave_google_api_key <brave_google_api_key>')
  .option('--brave_google_api_endpoint <brave_google_api_endpoint>')
  .option('--brave_infura_project_id <brave_infura_project_id>')
  .option('--bitflyer_client_id <bitflyer_client_id>')
  .option('--bitflyer_client_secret <bitflyer_client_secret>')
  .option('--bitflyer_staging_client_id <bitflyer_staging_client_id>')
  .option('--bitflyer_staging_client_secret <bitflyer_staging_client_secret>')
  .option('--bitflyer_staging_url <bitflyer_staging_url>')
  .option('--uphold_client_id <uphold_client_id>')
  .option('--uphold_client_secret <uphold_client_secret>')
  .option('--uphold_staging_client_id <uphold_staging_client_id>')
  .option('--uphold_staging_client_secret <uphold_staging_client_secret>')
  .option('--channel <target_channel>', 'target channel to build', /^(beta|dev|nightly|release)$/i)
  .option('--build_omaha', 'build omaha stub/standalone installer')
  .option('--tag_ap <ap>', 'ap for stub/standalone installer')
  .option('--skip_signing', 'skip signing dmg/brave_installer.exe')
  .option('--build_delta_installer', 'build delta mini installer')
  .option('--last_chrome_installer <last_chrome_installer>', 'folder contains previous version uncompressed chrome.7z pack file. This folder should be in out dir.')
  .option('--android_override_version_name <android_override_version_name>', 'Android version number')
  .option('--brave_safetynet_api_key <brave_safetynet_api_key>')
  .option('--notarize', 'notarize the macOS app with Apple')
  .option('--target_os <target_os>', 'target OS')
  .option('--target_arch <target_arch>', 'target architecture')
  .option('--target_android_base <target_android_base>', 'target Android SDK level for apk or aab (classic, modern, mono)', 'classic')
  .option('--target_android_output_format <target_android_output_format>', 'target Android output format (apk, aab)', 'aab')
  .option('--universal', 'build a universal binary distribution')
  .option('--is_asan', 'is asan enabled')
  .option('--use_goma [arg]', 'whether to use Goma for building', JSON.parse)
  .option('--goma_offline', 'use offline mode for goma')
  .option('--force_gn_gen', 'always run gn gen')
  .arguments('[build_config]')
  .action(createDist)

program
  .command('start')
  .allowUnknownOption(true)
  .option('-C <build_dir>', 'build config (out/Debug, out/Release')
  .option('--v [log_level]', 'set log level to [log_level]', parseInteger, '0')
  .option('--vmodule [modules]', 'verbose log from specific modules')
  .option('--user_data_dir_name [base_name]', 'set user data directory base name to [base_name]')
  .option('--no_sandbox', 'disable the sandbox')
  .option('--disable_brave_extension', 'disable loading the Brave extension')
  .option('--disable_brave_rewards_extension', 'disable loading the Brave Rewards extension')
  .option('--disable_pdfjs_extension', 'disable loading the PDFJS extension')
  .option('--disable_webtorrent_extension', 'disable loading the WebTorrent extension')
  .option('--ui_mode <ui_mode>', 'which built-in ui appearance mode to use', /^(dark|light)$/i)
  .option('--show_component_extensions', 'show component extensions in chrome://extensions')
  .option('--enable_brave_update', 'enable brave update')
  .option('--channel <target_channel>', 'target channel to start', /^(beta|dev|nightly|release)$/i, 'release')
  .option('--official_build <official_build>', 'force official build settings')
  // See https://github.com/brave/brave-browser/wiki/Rewards#flags for more information
  .option('--rewards [options]', 'options for rewards')
  .option('--brave_ads_testing', 'ads testing')
  .option('--brave_ads_production', 'ads production')
  .option('--brave_ads_staging', 'ads staging')
  .option('--brave_ads_debug', 'ads debug')
  .option('--single_process', 'use a single process')
  .option('--output_path [pathname]', 'use the Brave binary located at [pathname]')
  .arguments('[build_config]')
  .action(start.bind(null, parsedArgs.unknown))

program
  .command('pull_l10n')
  .option('--extension <extension>', 'Scope this command to localize a Brave extension such as greaselion')
  .option('--grd_path <grd_path>', `Relative path to match end of full GRD path, e.g: 'generated_resources.grd'.`)
  .option('--debug', `Dumps downloaded content for one language into TransifexCurrent.txt file in the temp directory.`)
  .action(pullL10n)

program
  .command('push_l10n')
  .option('--extension <extension>', 'Scope this command to localize a Brave extension such as greaselion')
  .option('--extension_path <extension_path>', 'Local path for extension')
  .option('--grd_path <grd_path>', `Relative path to match end of full GRD path, e.g: 'generated_resources.grd'.`)
  .option('--with_translations', 'Push local translations. WARNING: this will overwrite translations in Tansifex.')
  .option('--with_missing_translations', 'Push local translations for strings that do not have translations in Transifex.')
  .action(pushL10n)

program
  .command('delete_string_translations')
  .option(
    '--string_ids <string_ids>',
    'Transifex string hash IDs to clear translated values from all languages (comma separated), e.g: "647dde44fb7eb1e62cd502c4b1c25cb8,8b03500bd8f2fa55928521a68bed4b1b"',
    commaSeparatedList
  )
  .option(
    '--resource_name <resource_name>',
    'Transifex resource name from which to clear all translations for specified string IDs, e.g: "generated_resources"'
  )
  .action(l10nDeleteTranslations)

program
  .command('chromium_rebase_l10n')
  .action(chromiumRebaseL10n)

program
  .command('update_patches')
  .action(updatePatches)

program
  .command('cibuild')
  .option('--target_arch <target_arch>', 'target architecture')
  .action((options) => {
    build('Release', options)
  })

program
  .command('test <suite>')
  .allowUnknownOption(true)
  .option('-C <build_dir>', 'build config (out/Debug, out/Release')
  .option('--v [log_level]', 'set log level to [log_level]', parseInteger, '0')
  .option('--vmodule [modules]', 'verbose log from specific modules')
  .option('--filter <filter>', 'set test filter')
  .option('--output <output>', 'set test output (results) file path')
  .option('--disable_brave_extension', 'disable loading the Brave extension')
  .option('--single_process', 'uses a single process to run tests to help with debugging')
  .option('--test_launcher_jobs <test_launcher_jobs>', 'Number of jobs to launch', parseInteger, '4')
  .option('--target_os <target_os>', 'target OS')
  .option('--target_arch <target_arch>', 'target architecture')
  .option('--run_disabled_tests', 'run disabled tests')
  .option('--manual_android_test_device', 'indicates that Android test device is run manually')
  .option('--use_goma [arg]', 'whether to use Goma for building', JSON.parse)
  .option('--goma_offline', 'use offline mode for goma')
  .arguments('[build_config]')
  .action(test.bind(null, parsedArgs.unknown))

program
  .command('lint')
  .option('--base <base branch>', 'set the destination branch for the PR')
  .action(util.lint)

program
  .command('presubmit')
  .option('--base <base branch>', 'set the destination branch for the PR')
  .option('--all', 'run presubmit on all files')
  .option('--files <file list>',
    'semicolon-separated list files to run presubmit on')
  .option('--verbose [arg]', 'pass --verbose 2 for more debugging info', JSON.parse)
  .action(util.presubmit)

program
  .command('pylint')
  .option('--base <base_branch>', 'only analyse files changed relative to base_branch')
  .option('--all', 'run pylint on all files')
  .option('--report', 'produce a parseable report file')
  .action(pylint)

program
  .command('format')
  .option('--base <base branch>', 'set the destination branch for the PR')
  .option('--full', 'format all lines in changed files instead of only the changed lines')
  .option('--js', 'format javascript code with clang-format')
  .option('--python', 'enable formating of Python file types using yapf')
  .option('--rust', 'enables formatting of Rust file types using rustfmt')
  .option('--swift',
    'enables formatting of Swift file types using swift-format')
  .action(util.format)

program
  .command('mass_rename')
  .action(util.massRename)

program
  .parse(process.argv)
