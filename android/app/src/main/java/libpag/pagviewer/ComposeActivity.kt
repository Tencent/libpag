package libpag.pagviewer

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import org.libpag.PAGImageComponent

/**
 * Compose Demo
 */
class ComposeActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {

            Column(
                modifier = Modifier.fillMaxSize()
            ) {


                FlowRow(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxWidth()
                ) {


                    repeat(20) { i ->
                        PAGImageComponent(
                            modifier = Modifier
                                .padding(30.dp)
                                .size(60.dp),
                            path = "assets://list/$i.pag",
                            repeatCount = -1
                        )
                    }

                }




            }
        }
    }


}

@Composable
private fun ItemButton(
    modifier: Modifier = Modifier,
    text: String,
    onClick: () -> Unit
) {
    BasicText(
        text = text,
        color = { Color.White },
        modifier = modifier
            .background(color = Color.Blue)
            .padding(vertical = 8.dp, horizontal = 12.dp)
            .clickable(onClick = onClick)
    )
}