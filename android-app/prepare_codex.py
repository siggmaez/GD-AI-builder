"""Build Android Codex with vendored TLS and no phone command/file tools.

Account authentication and inference transport remain the upstream implementation.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1])
manifest = root / 'core/Cargo.toml'
text = manifest.read_text()
for target in ('aarch64-linux-android', 'x86_64-linux-android'):
    text += f'\n[target.{target}.dependencies]\nopenssl-sys = {{ workspace = true, features = ["vendored"] }}\n'
manifest.write_text(text)

# This companion is a JSON generator, not a phone coding terminal. Keep the
# upstream account/login/model implementation, but expose no executable tools.
spec = root / 'tools/src/tool_spec.rs'
text = spec.read_text()
for signature in ('pub fn create_tools_json_for_responses_api(', 'pub fn create_tools_json_for_responses_lite('):
    start = text.index(signature)
    brace = text.index(' {', start) + 2
    text = text[:brace] + '\n    if cfg!(target_os = "android") { return Ok(Vec::new()); }\n' + text[brace:]
start = text.index('pub fn create_tools_raw_json_for_responses_api(')
brace = text.index(' {', start) + 2
text = text[:brace] + '\n    if cfg!(target_os = "android") { return serde_json::value::to_raw_value(&Vec::<ToolSpec>::new()).map(Arc::from); }\n' + text[brace:]
spec.write_text(text)
router = root / 'core/src/tools/router.rs'
text = router.read_text()
signature = '    async fn dispatch_tool_call_with_code_mode_result_inner('
start = text.index(signature)
brace = text.index(' {', start) + 2
text = text[:brace] + '\n        if cfg!(target_os = "android") { return Err(FunctionCallError::RespondToModel("Phone bridge only returns JSON; device tools are disabled.".to_string())); }\n' + text[brace:]
router.write_text(text)
