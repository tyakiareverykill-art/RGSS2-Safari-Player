require "zlib"

STDOUT.sync = true
puts "CRuby #{RUBY_VERSION} / スクリプト検証開始"
begin
  scripts = Marshal.load(STDIN.binmode.read)
  raise TypeError unless scripts.instance_of?(Array) && scripts.size.between?(1, 16_384)
rescue StandardError => error
  abort "Marshal 復元失敗 / #{error.class}"
end
puts "Marshal 復元成功 / #{scripts.size} 件"

total = 0
compiled = 0
empty = 0
failed = 0
scripts.each_with_index do |row, index|
  unless row.instance_of?(Array) && row[0].is_a?(Integer) &&
         row[1].instance_of?(String) && row[2].instance_of?(String)
    abort "スクリプト形式不一致 / #{index + 1} 件目"
  end

  source = "".b
  unless row[2].empty?
    inflater = Zlib::Inflate.new
    begin
      packed = row[2]
      (0...packed.bytesize).step(256) do |offset|
        chunk = inflater.inflate(packed.byteslice(offset, 256))
        raise RangeError if source.bytesize + chunk.bytesize > 16 * 1024 * 1024
        source << chunk
      end
      raise Zlib::DataError unless inflater.finished? && inflater.total_in == row[2].bytesize
    rescue Zlib::Error, RangeError => error
      abort "スクリプト展開失敗 / #{index + 1} 件目 / #{error.class}"
    ensure
      inflater.close
    end
  end

  total += source.bytesize
  abort "スクリプト展開量が上限を超えました" if total > 128 * 1024 * 1024
  if source.empty?
    empty += 1
    next
  end

  begin
    source.force_encoding("UTF-8")
    source.force_encoding("Windows-31J") unless source.valid_encoding?
    RubyVM::InstructionSequence.compile(source, "RGSS section #{index + 1}")
    compiled += 1
  rescue SyntaxError, EncodingError, ArgumentError => error
    failed += 1
    puts "構文確認失敗 / #{index + 1} 件目 / #{error.class}" if failed <= 8
  end
end
puts "全スクリプト展開完了 / #{scripts.size} 件 / #{total} バイト"
puts "CRuby 構文確認 / 成功 #{compiled} / 空 #{empty} / 失敗 #{failed}"
exit(failed.zero? ? 0 : 2)
